/*
 Copyright (c) 2018-2020, Rice University 
 RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license

---------------------------------------------------------------------
 Event based message queue thread class for the recorder worker
---------------------------------------------------------------------
*/

#include "include/recorder_thread.h"
#include "include/logger.h"
#include "include/utils.h"
#include "include/macros.h"

namespace Sounder
{
    static const size_t kQueueSize = 36;

    RecorderThread::RecorderThread(Config* in_cfg, size_t buffer_size, size_t antenna_offset, size_t num_antennas, int tid, int core ) : 
        worker_(in_cfg, antenna_offset, num_antennas),
        cfg_(in_cfg)
    {
        buffer_size_ = buffer_size;
        worker_.init();
        event_queue_ = moodycamel::ConcurrentQueue<RecordEventData>(buffer_size_ * kQueueSize);

        MLPD_INFO("Launching recorder task thread with id: %d and core %d\n", tid, core);
        this->thread_ = std::thread(&RecorderThread::doRecording, this, tid, core);
    }

    RecorderThread::~RecorderThread()
    {
        finalize();
    }

    void RecorderThread::finalize( void )
    {
        //Wait for thread to cleanly finish the messages in the queue
        if (this->thread_.joinable() == true)
        {
            MLPD_TRACE("Joining Recorder Thread\n");
            this->thread_.join();
        }
        this->worker_.finalize();
    }

    /* TODO:  handle producer token better */
    //Returns true for success, false otherwise
    bool RecorderThread::dispatchWork(RecordEventData event)
    {
        //MLPD_TRACE("Dispatching work\n");
        moodycamel::ProducerToken ptok(this->event_queue_);
        bool ret = true;
        if (this->event_queue_.try_enqueue(ptok, event) == 0) {
            MLPD_WARN("Queue limit has reached! try to increase queue size.\n");
            if (this->event_queue_.enqueue(ptok, event) == 0) {
                MLPD_ERROR("Record task enqueue failed\n");
                throw std::runtime_error("Record task enqueue failed");
                ret = false;
            }
        }
        return ret;
    }

    void RecorderThread::doRecording(int tid, int core_id)
    {
        if (this->cfg_->core_alloc() == true) {
            MLPD_INFO("Pinning recording thread %d to core %d\n", tid, core_id + tid);
            pthread_t this_thread = this->thread_.native_handle();
            if (pin_thread_to_core((core_id + tid), &this_thread) != 0) {
                MLPD_ERROR("Pin recording thread %d to core %d failed\n", tid, core_id + tid);
                throw std::runtime_error("Pin recording thread to core failed");
            }
        }

        moodycamel::ConsumerToken ctok(this->event_queue_);
        MLPD_INFO("Recording thread %d has %zu antennas starting at %zu\n", tid, this->worker_.num_antennas(), this->worker_.antenna_offset());

        RecordEventData event;
        bool ret = false;
        while (this->cfg_->running() == true) {
            ret = this->event_queue_.try_dequeue(event);

            if (ret == true)
            {
                this->handleEvent(event, tid);
            }
        }
    }

    void RecorderThread::handleEvent(RecordEventData event, int tid)
    {
        size_t offset        = event.data;
        size_t buffer_id     = (offset / event.rx_buff_size);
        size_t buffer_offset = offset - (buffer_id * event.rx_buff_size);

        if (event.event_type == TASK_RECORD)
        {
            // read info
            size_t package_length
                = sizeof(Package) + this->cfg_->getPackageDataLength();
            char* cur_ptr_buffer = event.rx_buffer[buffer_id].buffer.data()
                + (buffer_offset * package_length);

            this->worker_.record(tid, reinterpret_cast<Package*>(cur_ptr_buffer));
        }

        /* Free up the buffer memory */
        int bit = 1 << (buffer_offset % sizeof(std::atomic_int));
        int offs = (buffer_offset / sizeof(std::atomic_int));
        std::atomic_fetch_and(
            &event.rx_buffer[buffer_id].pkg_buf_inuse[offs], ~bit); // now empty
    }
}; //End namespace Sounder