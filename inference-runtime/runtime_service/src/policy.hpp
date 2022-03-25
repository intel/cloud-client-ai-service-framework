// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <mutex> 
#include <ctime>

#define PATHNAME       "/etc"
#define PROJ_ID        0x745
#define CPU_THRESHOLD  0.05  //idle percentage < 5%
#define GPU_THRESHOLD  95    //idle percentage < 5%

#define BUF_SIZE       100

struct CfgParams {
    bool OffloadToLocal;
    bool OffloadToServer;
    std::string InferDevice;
    long SamplingInterval;
};

struct XPUStatus {
    bool CPUBusy;
    bool GPUBusy;
};

struct cpu_occupation {
    float idle_occupation;
};

struct ShareMemory {
    //CPU
    struct cpu_occupation cpuStatus;
    bool   cfgLocal;
    bool   cfgServer;
    char   cfgInferDev[BUF_SIZE];
    float  gpuStatus;    //busy
    int    pidOfDaemon;
};

class Policy {


public:
    Policy();
    Policy(std::string& cfgFile);
    int ReadShareMemory(struct CfgParams& cfgParam, struct XPUStatus& xpuStatus);
    int ParseConfiguration(const char* configuration_filename, struct CfgParams &Params);
    struct CfgParams GetCfgParameters();
    std::string GetCfgFile();
    bool IsCfgFileChanged(std::string cfgFile);
    int SaveCfgParams(struct CfgParams& cfgParam, struct ShareMemory* buf);
    int UserSetCfgParams(bool& offload, std::string& inferDevice);
    int GetInferDevice(std::string& inferDevice);

    int CreateShm(int size);
    int DestroyShm(int shmid);
    int GetShmid(int size);
    bool IsAvailableDevice(std::string device);
    int CheckSymbolic(const char* filename);
    int WakeupDaemon();

private:
    int CommShm(int size, int shmflag);
    struct CfgParams mCfgParams;
    std::string mCfgFile;
    time_t mModifyTime;
};
