// Copyright (C) 2020 Intel Corporation
//
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>

#include "policy.hpp"
#include "cpu_state.hpp"

// Returns true if an UTF-8 string only consists of ASCII characters
static bool IsASCII(std::string text) {

    for (std::string::const_iterator it = text.cbegin(); it != text.cend(); ++it) {
        const unsigned char kMinASCII = 1;
        const unsigned char kMaxASCII = 127;

        unsigned char character = static_cast<unsigned char>(*it);
        if (character < kMinASCII || character > kMaxASCII) {
            return false;
        }
    }

    return true;
}

Policy::Policy() {

}

Policy::Policy(std::string& cfgFile) {

    mCfgParams.OffloadToLocal = true;
    mCfgParams.OffloadToServer = false;
    mCfgParams.InferDevice = "CPU";
    mCfgParams.SamplingInterval = 500;
    mCfgFile = cfgFile;
    memset(&mModifyTime, 0, sizeof(mModifyTime));

    struct stat result;
    if(stat(cfgFile.c_str(), &result)==0) {
        mModifyTime = result.st_mtime;
    }

    ParseConfiguration(cfgFile.c_str(), mCfgParams);
}

int Policy::CommShm(int size, int shmflag) {
    key_t key = ftok(PATHNAME, PROJ_ID);
    if(key < 0){
        return -1;
    }
    int shmid = shmget(key,  size, shmflag);
    if(shmid < 0){
        return -2;
    }

    return shmid;
}

int Policy::CreateShm(int size) {
    int shmid = GetShmid(size);
    if(shmid < 0) {
        shmid = CommShm(size, IPC_CREAT|IPC_EXCL|0666);
    }

    return shmid;
}

int Policy::DestroyShm(int shmid) {

    if(shmctl(shmid, IPC_RMID, NULL) < 0) {
        return -1;
     }

    return 0;
}

int Policy::GetShmid(int size) {
    return CommShm(size, 0);
}

int Policy::ReadShareMemory(struct CfgParams& cfgParam, struct XPUStatus& xpuStatus) {

    int shmid = GetShmid(sizeof(struct ShareMemory));
    if (shmid < 0) {
        return -1;
    }

    struct ShareMemory* buf = reinterpret_cast<struct ShareMemory*>(shmat(shmid,NULL, 0));
    struct cpu_occupation sample_occupy = buf->cpuStatus;
    float gpuStatus = buf->gpuStatus;

    cfgParam.OffloadToLocal = buf->cfgLocal;
    cfgParam.OffloadToServer = buf->cfgServer;
    cfgParam.InferDevice = buf->cfgInferDev;
    cfgParam.SamplingInterval = 500;

    //detech share memory
    if(shmdt(reinterpret_cast<void*>(buf)) == -1) {
        syslog(LOG_ERR, "AI-Service-Framework: ERROR: detach share memory error!");
    }
    std::cout <<"cpu idle:" << sample_occupy.idle_occupation << ",gpu:" << gpuStatus << std::endl;

    if(sample_occupy.idle_occupation < CPU_THRESHOLD) {
        syslog(LOG_NOTICE, "AI-Service-Framework: CPU busy!");
        xpuStatus.CPUBusy = true;
    }

    if(gpuStatus > GPU_THRESHOLD) {
        syslog(LOG_NOTICE, "AI-Service-Framework: GPU busy!");
        xpuStatus.GPUBusy = true;
    }

    return 0;
}

int Policy::SaveCfgParams(struct CfgParams& cfgParam, struct ShareMemory* buf) {

    if (!buf) {
        return -1;
    }

    buf->cfgLocal = cfgParam.OffloadToLocal;
    buf->cfgServer = cfgParam.OffloadToServer;
    memset(buf->cfgInferDev, 0, sizeof(buf->cfgInferDev));
    size_t copyLength = std::min(sizeof(buf->cfgInferDev) - 1, cfgParam.InferDevice.length());
    memcpy(buf->cfgInferDev, cfgParam.InferDevice.c_str(), copyLength);
    buf->cfgInferDev[copyLength] = '\0';

    return 0;
}

int Policy::UserSetCfgParams(bool& offload, std::string& inferDevice) {

    int shmid = GetShmid(sizeof(struct ShareMemory));
    if (shmid < 0) {
        return -1;
    }

    struct ShareMemory* buf = reinterpret_cast<struct ShareMemory*>(shmat(shmid,NULL, 0));

    //do inference locally if offload == true
    if (offload) {
        buf->cfgLocal  = true;
        buf->cfgServer = false;
    } else {
        buf->cfgLocal  = false;
        buf->cfgServer = true;
    }

    std::string oldInferDevice = buf->cfgInferDev;
    if (oldInferDevice != inferDevice) {
        syslog(LOG_NOTICE, "AI-Service-Framework: infer device is changed");
        memset(buf->cfgInferDev, 0, sizeof(buf->cfgInferDev));
        size_t copyLength = std::min(sizeof(buf->cfgInferDev) - 1, inferDevice.length());
        memcpy(buf->cfgInferDev, inferDevice.c_str(), copyLength);
        buf->cfgInferDev[copyLength] = '\0';
    }

    //detech share memory
    if(shmdt(reinterpret_cast<void*>(buf)) == -1) {
        syslog(LOG_ERR, "AI-Service-Framework: failed to detach share memory !");
    }

    return 0;
}

int Policy::ParseConfiguration(const char* configuration_filename, struct CfgParams& Params) {

    if (access(configuration_filename, 0) != 0) { // mode = 0: whether file exist
        syslog(LOG_ERR, "AI-Service-Framework: configuration file doesn't exist! ");
        return -1;
    }
    if (CheckSymbolic(configuration_filename) == -1) {
        syslog(LOG_ERR, "AI-Service-Framework: ERROR: configuration file is a symlink. Refuse opening it!");
	return -1;
    }
    std::ifstream file(configuration_filename);
    if (!file.is_open()) {
        syslog(LOG_ERR, "AI-Service-Framework: Failed to open configuration file: %s", configuration_filename);
        return -1;
    }

    std::string line;
    while (std::getline(file, line))  //no endline symbol "\n"
    {
        std::istringstream line_stream(line);
        line.erase(std::find_if(line.rbegin(), line.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), line.end());

        // empty line and comment is a valid case
        if (line.empty() || line.rfind("#", 0) == 0) {
            continue;
        }

        if (!IsASCII(line)) {
            syslog(LOG_ERR, "AI-Service-Framework: Non-ASCII character found in configuration file. %s : %s",
                   configuration_filename, line.c_str());
            return -1;
        }

        const char COLUMN_SEPARATOR = ' ';
        if (line.find(COLUMN_SEPARATOR) == std::string::npos) {
            syslog(LOG_ERR, "AI-Service-Framework: Invalid format of configuration file: %s", configuration_filename);
            return -1;
        }

        std::string param_name, param_value;
        std::getline(line_stream, param_name, COLUMN_SEPARATOR);
        std::getline(line_stream, param_value, COLUMN_SEPARATOR);
        param_value.erase(std::find_if(param_value.rbegin(), param_value.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), param_value.end());

        if (param_name.empty())
        {
            syslog(LOG_ERR, "AI-Service-Framework: Invalid format of configuration file: %s", configuration_filename);
            return -1;
        }

        try
        {
            if (param_name == "-Offload-to-local") {
                if (param_value == "LOCAL_ONLY") {
                    mCfgParams.OffloadToLocal = true;
                    mCfgParams.OffloadToServer = false;
                } else if (param_value == "LOCAL_FIRST") {
                   mCfgParams.OffloadToLocal = true;
                   mCfgParams.OffloadToServer = true;
                } else if (param_value == "SERVER_ONLY") {
                    mCfgParams.OffloadToLocal = false;
                    mCfgParams.OffloadToServer = true;
                } else {
                    syslog(LOG_ERR, "AI-Service-Framework: offload param setting error!\n");
                }
            } else if (param_name == "-Device") {
                std::cerr << "device: " << param_value << std::endl;
                mCfgParams.InferDevice = param_value;
            } else if (param_name == "-Local-sampling") {
                std::cerr << "sampling: " << param_value << std::endl;
                mCfgParams.SamplingInterval = std::stoi(param_value);
            }

        }
        catch (const std::invalid_argument &) {
            syslog(LOG_ERR, "AI-Service-Framework: Invalid value of parameter: %s", param_name.c_str());
            return -1;
        }
        catch (const std::out_of_range &) {
            syslog(LOG_ERR, "AI-Service-Framework: Value of parameter out of range: %s", param_name.c_str());
            return -1;
        }
    }

    file.close();
    Params =  mCfgParams;

    return 0;
}

struct CfgParams Policy::GetCfgParameters() {
    return  mCfgParams;
}

std::string Policy::GetCfgFile() {
    return mCfgFile;
}

int Policy::GetInferDevice(std::string& inferDevice) {

    int shmid = GetShmid(sizeof(struct ShareMemory));
    if (shmid < 0) {
        syslog(LOG_ERR, "AI-Service-Framework: unable to get share memeory");
        return -1;
    }

    struct ShareMemory* buf = reinterpret_cast<struct ShareMemory*>(shmat(shmid,NULL, 0));

    inferDevice = buf->cfgInferDev;

    if(shmdt(reinterpret_cast<void*>(buf)) == -1) {
        syslog(LOG_ERR, "AI-Service-Framework: failed to detach share memory error!");
    }

    return 0;
}

bool Policy::IsCfgFileChanged(std::string cfgFile) {
    bool res = false;
    struct stat result;
    if(stat(cfgFile.c_str(), &result)==0) {
        auto modify_time = result.st_mtime;
        if (modify_time != mModifyTime) {
            std::cout << "config file is modified!" << std::endl;
            mModifyTime = modify_time;
            res = true;
        }
    } else {
        syslog(LOG_ERR, "AI-Service-Framework: config file status is changed!");
        res = true;
    }

    return res;
}

bool Policy::IsAvailableDevice(std::string device) {

    std::vector<std::string> support_devices = {
        "CPU",
        "GPU",
        "GNA_AUTO",
        "GNA_HW",
        "GNA_SW_EXACT",
        "GNA_SW",
        "GNA_SW_FP32",
        "HETERO:GNA,CPU",
        "HETERO:GNA_HW,CPU",
        "HETERO:GNA_SW_EXACT,CPU",
        "HETERO:GNA_SW,CPU",
        "HETERO:GNA_SW_FP32,CPU",
        "HETERO:CPU,GPU",
        "HETERO:GPU,CPU",
        "MYRIAD"

    };

    if (std::find(support_devices.begin(), support_devices.end(), device)
        == support_devices.end()) {

        syslog(LOG_ERR, "AI-Service-Framework: Not a available device!");
        return false;
    }

    return true;
}

int Policy::CheckSymbolic(const char* filename) {

    struct stat lstat_val;
    struct stat fstat_val;

    /* collect any link info about the file */
    if (lstat(filename, &lstat_val) == -1) {
        return -1;
    }

    /* collect info about the real file */
    if (stat(filename, &fstat_val) == -1) {
        return -1;
    }

    /* we should not have followed a symbolic link */
    if (lstat_val.st_mode == fstat_val.st_mode &&
        lstat_val.st_ino == fstat_val.st_ino  &&
        lstat_val.st_dev == fstat_val.st_dev) {
        return 0;
    } else {
        return -1;
    }
}

int Policy::WakeupDaemon() {
    int shmid = GetShmid(sizeof(struct ShareMemory));
    if (shmid < 0) {
        syslog(LOG_ERR, "AI-Service-Framework: unable to get share memeory!");
        return -1;
    }

    struct ShareMemory* buf = reinterpret_cast<struct ShareMemory*>(shmat(shmid,NULL, 0));
    pid_t daemon_pid = buf->pidOfDaemon;

    //detech share memory
    if(shmdt(reinterpret_cast<void*>(buf)) == -1) {
        syslog(LOG_ERR, "AI-Service-Framework: failed to detach share memory!");
    }

    kill(daemon_pid, SIGUSR1);

    return 0;
}

