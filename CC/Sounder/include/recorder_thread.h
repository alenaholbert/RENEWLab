/*
 Copyright (c) 2018-2019 
 RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license
 Author(s): Peiyao Zhao 
            Rahman Doost-Mohamamdy, doost@rice.edu
 
----------------------------------------------------------------------
 Record received frames from massive-mimo base station in HDF5 format
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
        struct EventHandlerContext {
            RecorderThread*      me;
            size_t               id;
        };

        struct RecordEventData {
            int event_type;
            int data;
            SampleBuffer* rx_buffer;
            size_t rx_buff_size;
        };

        RecorderThread(Config* in_cfg, std::vector<unsigned> antennas);
        ~RecorderThread();

        pthread_t create(int tid);
        bool      dispatchWork(RecordEventData event);
    private:
        static void *launchThread(void *in_context);

        //Main threading loop        //Main threading loop
        void doRecording(int tid, int core_id);
        void handleEvent(RecordEventData event, int tid);
        int  join();
        int  finalize();

        //1 - Producer (dispatcher), 1 - Consumer
        moodycamel::ConcurrentQueue<RecordEventData> event_queue_;
        RecorderWorker worker_;
        Config* cfg_;
        pthread_t thread_;
    };
};

#endif /* SOUDER_RECORDER_THREAD_H_ */
