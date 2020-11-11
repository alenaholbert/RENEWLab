/*
 Copyright (c) 2018-2019 
 RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license
 Author(s): Peiyao Zhao 
            Rahman Doost-Mohamamdy, doost@rice.edu
 
----------------------------------------------------------------------
 Record received frames from massive-mimo base station in HDF5 format
---------------------------------------------------------------------
*/
#ifndef DATARECORDER_HEADER
#define DATARECORDER_HEADER

#include "receiver.h"
#include "recorder_worker.h"

class Recorder {
public:
    Recorder(Config* in_cfg);
    ~Recorder();

    void do_it();
    int getRecordedFrameNum();
    std::string getTraceFileName() { return this->cfg_->trace_file(); }

private:
    struct EventHandlerContext {
        Recorder* obj_ptr;
        size_t id;
    };
    void taskThread(EventHandlerContext* context);
    static void* taskThread_launch(void* in_context);
    void gc(void);

    // buffer length of each rx thread
    static const int kSampleBufferFrameNum;
    // dequeue bulk size, used to reduce the overhead of dequeue in main thread
    static const int KDequeueBulkSize;

    Config* cfg_;
    std::unique_ptr<Receiver> receiver_;
    SampleBuffer* rx_buffer_;
    size_t rx_thread_buff_size_;

    RecorderWorker worker_;
    size_t max_frame_number_;
#if DEBUG_PRINT
    hsize_t cdims_pilot[5];
    hsize_t cdims_data[5];
#endif

    moodycamel::ConcurrentQueue<Event_data> task_queue_;
    moodycamel::ConcurrentQueue<Event_data> message_queue_;
};
#endif /* DATARECORDER_HEADER */
