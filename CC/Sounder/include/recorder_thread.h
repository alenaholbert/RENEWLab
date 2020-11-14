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
#include <condition_variable>
#include <mutex>

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

        RecorderThread(Config* in_cfg, size_t buffer_size, size_t antenna_offset, size_t num_antennas);
        ~RecorderThread();

        void   Start(int tid, int core);
        bool   DispatchWork(RecordEventData event);
    private:
        //Main threading loop        //Main threading loop
        void DoRecording(int tid, int core_id);
        void HandleEvent(RecordEventData event, int tid);
        void Finalize();

        //1 - Producer (dispatcher), 1 - Consumer
        moodycamel::ConcurrentQueue<RecordEventData> event_queue_;
        RecorderWorker worker_;
        Config* cfg_;
        std::thread thread_;
        size_t buffer_size_;
        /* Delayed start of thread */
        std::mutex sync_;
        std::condition_variable condition_;
        bool running_;
    };
};

#endif /* SOUDER_RECORDER_THREAD_H_ */
