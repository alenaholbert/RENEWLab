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
    ~RecorderWorker();

    herr_t record(int tid, int antenna, Package *pkg);

    herr_t initHDF5(const std::string&);
    void openHDF5();
    void closeHDF5();
    void finishHDF5();
private:

    // pilot dataset size increment
    static const int kConfigPilotExtentStep;
    // data dataset size increment
    static const int kConfigDataExtentStep;

    void gc( void );

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
};

#endif /* RECORDER_WORKER_HEADER */
