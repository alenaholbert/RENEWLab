/*
 Copyright (c) 2018-2019, Rice University 
 RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license

----------------------------------------------------------------------
 Record received frames from massive-mimo base station in HDF5 format
---------------------------------------------------------------------
*/

#include "include/recorder_thread.h"
#include "include/logger.h"
#include "include/utils.h"
#include "include/macros.h"

namespace Sounder
{
    static const size_t kQueueSize = 36;

    RecorderThread::RecorderThread(Config* in_cfg, size_t buffer_size, std::vector<unsigned> antennas) : 
        worker_(in_cfg, antennas),
        cfg_(in_cfg),
        thread_(0)
    {
        buffer_size_ = buffer_size;
        worker_.init();
        event_queue_ = moodycamel::ConcurrentQueue<RecordEventData>(buffer_size_ * kQueueSize);
        /* Should we create the thread here?  maybe */
    }

    RecorderThread::~RecorderThread()
    {
        finalize();
    }

    //TODO move to std::thread or allow a attr to be passed in
    // should the tid be private member, and create thread takes no params??
    pthread_t RecorderThread::create(int tid)
    {
        assert(this->thread_ == 0); //Cannot call 2 times.
        pthread_attr_t joinable_attr;
        pthread_attr_init(&joinable_attr);
        pthread_attr_setdetachstate(&joinable_attr, PTHREAD_CREATE_JOINABLE); //DETACHED

        RecorderThread::EventHandlerContext* context = new RecorderThread::EventHandlerContext();
        context->me = this;
        context->id = tid;
        MLPD_TRACE("Launching recorder task thread with id: %d\n", tid);
        if (pthread_create(&this->thread_, &joinable_attr,
                RecorderThread::launchThread, context)
            != 0) {
            delete context;
            throw std::runtime_error("Recorder task thread create failed");
        }
        return this->thread_;
    }

    int RecorderThread::finalize( void )
    {
        //Wait for thread to cleanly finish the messages in the queue
        int ret = this->join();
        //Close the files nicely
        this->worker_.finalize();
        return ret;
    }

    int RecorderThread::join( void )
    {
        MLPD_TRACE("Joining Recorder Thread %d\n", (int)this->thread_);
        return pthread_join(this->thread_, NULL);
    }


    /* Static for all instances */
    void* RecorderThread::launchThread(void *in_context)
    {
        EventHandlerContext* context = reinterpret_cast<EventHandlerContext*>(in_context);
        
        auto me  = context->me;
        auto tid = context->id;
        delete context;
        me->doRecording(tid, 0);
        return nullptr;
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
            if (pin_to_core(core_id + tid) != 0) {
                MLPD_ERROR("Pin recording thread %d to core %d failed\n", tid, core_id + tid);
                throw std::runtime_error("Pin recording thread to core failed");
            }
        }

        moodycamel::ConsumerToken ctok(this->event_queue_);
        MLPD_TRACE("Recording thread: %d started\n", tid);

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