/*
 Copyright (c) 2018-2019, Rice University 
 RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license

----------------------------------------------------------------------
 Record received frames from massive-mimo base station in HDF5 format
---------------------------------------------------------------------
*/

#include "include/recorder.h"
#include "include/logger.h"
#include "include/macros.h"
#include "include/signalHandler.hpp"
#include "include/utils.h"

// buffer length of each rx thread
const int Recorder::kSampleBufferFrameNum = 80;
// dequeue bulk size, used to reduce the overhead of dequeue in main thread
const int Recorder::KDequeueBulkSize = 5;


#if (DEBUG_PRINT)
const int kDsSim = 5;
#endif


Recorder::Recorder(Config* in_cfg) : cfg_(in_cfg), worker_(in_cfg)
{
    size_t rx_thread_num = cfg_->rx_thread_num();
    size_t ant_per_rx_thread
        = cfg_->bs_present() ? cfg_->getTotNumAntennas() / rx_thread_num : 1;
    rx_thread_buff_size_
        = kSampleBufferFrameNum * cfg_->symbols_per_frame() * ant_per_rx_thread;

    //task_queue_
    //    = moodycamel::ConcurrentQueue<Event_data>(rx_thread_buff_size_ * 36);
    message_queue_
        = moodycamel::ConcurrentQueue<Event_data>(rx_thread_buff_size_ * 36);

    MLPD_TRACE("Recorder construction - rx thread: %zu, task tread %zu, chunk "
               "size: %zu\n",
        rx_thread_num, task_thread_num, rx_thread_buff_size_);

    if (rx_thread_num > 0) {
        // initialize rx buffers
        rx_buffer_ = new SampleBuffer[rx_thread_num];
        size_t intsize = sizeof(std::atomic_int);
        size_t arraysize = (rx_thread_buff_size_ + intsize - 1) / intsize;
        size_t packageLength = sizeof(Package) + cfg_->getPackageDataLength();
        for (size_t i = 0; i < rx_thread_num; i++) {
            rx_buffer_[i].buffer.resize(rx_thread_buff_size_ * packageLength);
            rx_buffer_[i].pkg_buf_inuse = new std::atomic_int[arraysize];
            std::fill_n(rx_buffer_[i].pkg_buf_inuse, arraysize, 0);
        }
    }

    // Receiver object will be used for both BS and clients
    try {
        receiver_.reset(new Receiver(rx_thread_num, cfg_, &message_queue_));
    } catch (std::exception& e) {
        std::cout << e.what() << '\n';
        gc();
        throw runtime_error("Error Setting up the Receiver");
    }
}

void Recorder::gc(void)
{
    MLPD_TRACE("Garbage collect\n");
    this->receiver_.reset();
    if (this->cfg_->rx_thread_num() > 0) {
        for (size_t i = 0; i < this->cfg_->rx_thread_num(); i++) {
            delete[] this->rx_buffer_[i].pkg_buf_inuse;
        }
        delete[] this->rx_buffer_;
    }
}

Recorder::~Recorder() { this->gc(); }

void Recorder::do_it()
{
    pthread_t worker_thread;

    MLPD_TRACE("Recorder work thread\n");
    if ((this->cfg_->core_alloc() == true) && (pin_to_core(0) != 0)) {
        MLPD_ERROR("pinning main thread to core 0 failed");
        throw std::runtime_error("Pinning main thread to core 0 failed");
    }

    if (this->cfg_->client_present() == true) {
        auto client_threads = this->receiver_->startClientThreads();
    }

    if (this->cfg_->rx_thread_num() > 0) {
        //This is the spot that will create the necessary workers and split up the work
        std::vector<unsigned> antennas;
        for (size_t i = 0; i < cfg_->getTotNumAntennas(); i++)
        {
            antennas.push_back(i);
        }
        worker_.init(antennas);

        size_t task_thread_num = cfg_->task_thread_num();
        if (task_thread_num > 0) {
            pthread_attr_t detached_attr;
            pthread_attr_init(&detached_attr);
            pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);

            //Configure the worker threads.
            // TODO: add support for multiple workers
            //for (size_t i = 0; i < task_thread_num; i++) {
            RecorderWorker::EventHandlerContext* context = new RecorderWorker::EventHandlerContext();
            context->me = &this->worker_;
            context->id = 0;
            MLPD_TRACE("Launching recorder task thread with id: %zu\n", i);
            if (pthread_create(&worker_thread, &detached_attr,
                    RecorderWorker::launchThread, context)
                != 0) {
                delete context;
                gc();
                throw runtime_error("Recorder task thread create failed");
            }
        }

        // create socket buffer and socket threads
        auto recv_thread
            = this->receiver_->startRecvThreads(this->rx_buffer_, 1);
    } else
        this->receiver_->go(); // only beamsweeping

    moodycamel::ConsumerToken ctok(this->message_queue_);

    Event_data events_list[KDequeueBulkSize];
    int ret = 0;

    /* TODO : we can probably remove the dispatch function and pass directly to the recievers */
    while ((this->cfg_->running() == true) && (SignalHandler::gotExitSignal() == false)) {
        // get a bulk of events from the receivers
        ret = this->message_queue_.try_dequeue_bulk(
            ctok, events_list, KDequeueBulkSize);
        //if (ret > 0)
        //{
        //    MLPD_TRACE("Message(s) received: %d\n", ret );
        //}
        // handle each event
        for (int bulk_count = 0; bulk_count < ret; bulk_count++) {
            Event_data& event = events_list[bulk_count];

            // if EVENT_RX_SYMBOL, dispatch to proper worker
            if (event.event_type == EVENT_RX_SYMBOL) {
                int offset = event.data;
                RecorderWorker::RecordEventData do_record_task;
                do_record_task.event_type   = TASK_RECORD;
                do_record_task.data         = offset;
                do_record_task.rx_buffer    = this->rx_buffer_;
                do_record_task.rx_buff_size = this->rx_thread_buff_size_;
                // Pass the work off to the applicable worker
                // Worker must free the buffer, future work could involve making this cleaner

                //If no worker threads, it is possible to handle the event directly.
                //this->worker_.handleEvent(do_record_task, 0);
                if ( this->worker_.dispatchWork(do_record_task) == false )
                {
                    MLPD_ERROR("Record task enqueue failed\n");
                    throw std::runtime_error("Record task enqueue failed");
                }
            }
        }
    }
    this->cfg_->running(false);
    /* TODO: should we wait for the receive threads to terminate nicely? (recv_thread) */
    this->receiver_.reset();

    /* TODO join the worker threads before finalizing. */
    pthread_join(worker_thread, NULL);
    if (this->cfg_->rx_thread_num() > 0) {
        this->worker_.finalize();
    }
}


int Recorder::getRecordedFrameNum() { return this->max_frame_number_; }

extern "C" {
Recorder* Recorder_new(Config* in_cfg)
{
    Recorder* rec = new Recorder(in_cfg);
    return rec;
}

void Recorder_start(Recorder* rec) { rec->do_it(); }
int Recorder_getRecordedFrameNum(Recorder* rec)
{
    return rec->getRecordedFrameNum();
}
const char* Recorder_getTraceFileName(Recorder* rec)
{
    return rec->getTraceFileName().c_str();
}
}
