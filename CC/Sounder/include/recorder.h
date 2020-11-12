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
#include "recorder_thread.h"

class Recorder {
public:
    Recorder(Config* in_cfg);
    ~Recorder();

    void do_it();
    int getRecordedFrameNum();
    std::string getTraceFileName() { return this->cfg_->trace_file(); }

private:
    void gc(void);

    // buffer length of each rx thread
    static const int kSampleBufferFrameNum;
    // dequeue bulk size, used to reduce the overhead of dequeue in main thread
    static const int KDequeueBulkSize;

    Config* cfg_;
    std::unique_ptr<Receiver> receiver_;
    SampleBuffer* rx_buffer_;
    size_t rx_thread_buff_size_;

    //RecorderWorker worker_;
    std::vector<Sounder::RecorderThread *> recorders_;
    size_t max_frame_number_;

    moodycamel::ConcurrentQueue<Event_data> message_queue_;
};
#endif /* DATARECORDER_HEADER */
