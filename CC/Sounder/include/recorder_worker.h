/*
 Copyright (c) 2018-2019 
 RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license
 Author(s): Peiyao Zhao 
            Rahman Doost-Mohamamdy, doost@rice.edu
 
----------------------------------------------------------------------
 Record received frames from massive-mimo base station in HDF5 format
---------------------------------------------------------------------
*/
#ifndef RECORDER_WORKER_HEADER
#define RECORDER_WORKER_HEADER

#include "H5Cpp.h"
#include "config.h"
#include "receiver.h"

class RecorderWorker
{
public:
    RecorderWorker(Config* in_cfg);
    RecorderWorker(Config* in_cfg, const std::vector<unsigned>& antennas);
    ~RecorderWorker();

    struct EventHandlerContext {
        RecorderWorker*      me;
        size_t               id;
    };

    struct RecordEventData {
        int event_type;
        int data;
        SampleBuffer* rx_buffer;
        size_t rx_buff_size;
    };

    void init( const std::vector<unsigned>& antennas );
    void finalize( void );
    herr_t record(int tid, Package *pkg);

    /* Threading functions */
    static void *launchThread(void *in_context);

    //Main threading loop
    void doRecording(int tid, int core_id);
    //Dispatch work
    bool dispatchWork(RecordEventData event);
    //Tracks and frees the event memory buffer
    void handleEvent(RecordEventData event, int tid);

private:

    // pilot dataset size increment
    static const int kConfigPilotExtentStep;
    // data dataset size increment
    static const int kConfigDataExtentStep;

    void gc( void );
    herr_t initHDF5();
    void openHDF5();
    void closeHDF5();
    void finishHDF5();

    Config* cfg_;
    H5std_string hdf5_name_;

    H5::H5File* file_;
    // Group* group;
    H5::DSetCreatPropList pilot_prop_;
    H5::DSetCreatPropList data_prop_;

    H5::DataSet* pilot_dataset_;
    H5::DataSet* data_dataset_;

    size_t frame_number_pilot_;
    size_t frame_number_data_;

    size_t max_frame_number_;

    /* List of antennas the recorder will be responsable for */
    std::vector<unsigned> antennas_;

    //Threading helpers
    //1 - Producer (dispatcher), 1 - Consumer
    moodycamel::ConcurrentQueue<RecordEventData> event_queue_;
};

#endif /* RECORDER_WORKER_HEADER */
