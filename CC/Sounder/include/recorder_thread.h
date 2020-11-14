/*
 Copyright (c) 2018-2020
 RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license
 
----------------------------------------------------------------------
Event based message queue thread class for the recorder worker
---------------------------------------------------------------------
*/
#ifndef SOUDER_RECORDER_THREAD_H_
#define SOUDER_RECORDER_THREAD_H_

#include "recorder_worker.h"

namespace Sounder
{
    class RecorderThread
    {
    public:
        struct RecordEventData {
            int event_type;
            int data;
            SampleBuffer* rx_buffer;
            size_t rx_buff_size;
        };

        RecorderThread(Config* in_cfg, size_t buffer_size, size_t antenna_offset, size_t num_antennas, int tid, int core);
        ~RecorderThread();

        void   create(int tid, int core);
        bool   dispatchWork(RecordEventData event);
    private:
        //Main threading loop        //Main threading loop
        void doRecording(int tid, int core_id);
        void handleEvent(RecordEventData event, int tid);
        void finalize();

        //1 - Producer (dispatcher), 1 - Consumer
        moodycamel::ConcurrentQueue<RecordEventData> event_queue_;
        RecorderWorker worker_;
        Config* cfg_;
        std::thread thread_;
        size_t buffer_size_;
    };
};

#endif /* SOUDER_RECORDER_THREAD_H_ */
