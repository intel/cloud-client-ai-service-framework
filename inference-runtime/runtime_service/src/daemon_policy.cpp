// Copyright (C) 2020 Intel Corporation
//

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>

#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cmath>
#include <map>
#include <mutex>
#include <future>

#include "cpu_state.hpp"
#include "policy.hpp"

#define NODE_NUM            31
#define CHECK_TIME          5   //second
#define MIN_LINE_LEN        103
#define GPU_STATUS_HEADER   (MIN_LINE_LEN * 2)
#define RESTART_GPU_TIME    600

struct time_node {
    uint64_t cpu_busy_time;
    uint64_t cpu_idle_time;
    std::map<int, uint64_t> pss_time; //map<fcgi_pid, pss>
    float gpu_status;
};

struct nodeList {
    struct time_node node_array[NODE_NUM];
    int cur;
    int pre;
} node_list;

static bool gpuTopRunning = false;
static std::chrono::steady_clock::time_point check_gpu_start_time;

static int closeGpuTop();

static long gTimeout = 0;
static long gDefaultTimeout = 0;
std::mutex gMtx;
std::promise<bool> *gProm = nullptr;

static int get_process_occupation(struct time_node &node, std::vector<int>& process_id) {

    node.pss_time.clear();
    if (process_id.size() == 0) {
        std::cout << "process empty" << std::endl;
        return 0;
    }

    for (auto& pid : process_id) {
        uint64_t pss_time = 0;
        struct proc_self_stat pss;
        if (read_proc_self_stat(&pss, &pid) == 0) // read successfully
            pss_time = SELF_TIME(&pss);
        node.pss_time.insert({pid, pss_time});
    }

    return 0;
}

static float get_cpu_occupation(int indexA, int indexB, uint64_t &total_pss_time) {

    struct time_node cpu_time_info;
    struct time_node *pNodeA = &node_list.node_array[indexA];
    struct time_node *pNodeB = &node_list.node_array[indexB];

    cpu_time_info.cpu_busy_time =  pNodeB->cpu_busy_time - pNodeA->cpu_busy_time;
    cpu_time_info.cpu_idle_time =  pNodeB->cpu_idle_time - pNodeA->cpu_idle_time;

    total_pss_time = 0;
    for (auto iter = pNodeB->pss_time.begin(); iter != pNodeB->pss_time.end(); ++iter) {
        auto iterA = pNodeA->pss_time.find(iter->first);
        if ((iterA != pNodeA->pss_time.end()) && (iter->second > 0)) {
	    total_pss_time += (iter->second - iterA->second);
	} else {
            // Thread is restarted or terminated
	    total_pss_time += iter->second;
	}

    }

    float idle_percentage = -1;
    auto running_time = cpu_time_info.cpu_idle_time + cpu_time_info.cpu_busy_time;
    if ((indexA != indexB) && (running_time != 0)) {
        idle_percentage = (float)(cpu_time_info.cpu_idle_time + total_pss_time)
                          /(float)running_time;
    }

    return std::round(idle_percentage * 100)/100;
}

static int process_cpu_occupancy(struct cpu_occupation &sample_occupy,
                                 std::vector<int>& processes_id,
                                 uint64_t &total_pss) {

    struct proc_stat ps;

    if (read_proc_stat(&ps) < 0) {
         syslog(LOG_ERR, "AI-Service-Framework: ERROR: read cpu state");
        return -1;
    }
 
    int index = node_list.cur;
    node_list.node_array[index].cpu_busy_time = BUSY_TIME(&ps);
    node_list.node_array[index].cpu_idle_time = IDLE_TIME(&ps);
    get_process_occupation(node_list.node_array[index], processes_id);

    int ind_pre = node_list.pre;
    auto occupation = get_cpu_occupation(ind_pre, index, total_pss);
    if (occupation != -1)
        sample_occupy.idle_occupation = occupation;
    node_list.pre = index;
    node_list.cur = (node_list.cur + 1) % NODE_NUM;

    return 0;
}

static void signalHandler(int signum) {

    Policy shareMem;
    int shmid = shareMem.GetShmid(sizeof(struct ShareMemory));
    if (shmid >= 0 ) {
        shareMem.DestroyShm(shmid);
    }

    // close gputop
    if (gpuTopRunning) {
	closeGpuTop();
    }

    exit(signum);
}

static bool getFcgiIds(std::vector<int>& ids) {

    bool res = false;

    std::string cmd = "ps -aux | grep fcgi | grep -v grep";
    FILE *fp = popen(cmd.data(), "r");
    if (fp != NULL) {
        char buff[300];
        while (fgets(buff, 300, fp) != NULL) {
            const char COLUMN_SEPARATOR = ' ';
            std::string line = buff;
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](int ch) {
                       return !std::isspace(ch);}));
            std::string owner = line.substr(0, line.find(COLUMN_SEPARATOR));

            line.erase(0, line.find(COLUMN_SEPARATOR));
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](int ch) {
                       return !std::isspace(ch);}));
            std::size_t nPos = line.find(COLUMN_SEPARATOR);
            std::string thread_id = line.substr(0, nPos);

            if (!thread_id.empty()) {
                int id = std::stoi(thread_id);
                ids.push_back(id);
            }

            res = true;
        }
        pclose(fp);
        fp = NULL;
    } else {
        syslog(LOG_ERR, "AI-Service-Framework: get thread ids error");
    }

    return res;
}

static int getProcessPidByName(const char *pid_name, std::vector<pid_t> &pids) {

     FILE *fp;
     char buf[100];
     char cmd[200] = {'\0'};
     int ret = -1;
     snprintf(cmd, 200, "pidof %s", pid_name);

     if((fp = popen(cmd, "r")) != NULL) {
         if(fgets(buf, 100, fp) != NULL) {
             std::istringstream iss;
             iss.str(buf);
             pid_t pid = -1;
             while(iss >> pid) {
                pids.push_back(pid);
                ret = 0;
             }
         }
         pclose(fp);
         fp = NULL;
     }

     return ret;
}

static int closeGpuTop() {

    std::vector<pid_t> gpuPids;
    int ret = getProcessPidByName("intel_gpu_top", gpuPids);
    if (ret == 0) {
        for (size_t i = 0; i < gpuPids.size(); i++) {
            char cmd[200] = {'\0'};
            snprintf(cmd, 200, "sudo kill -9 %d", gpuPids[i]);
            ret = system(cmd);
        }
    }

    if (ret == -1)
        syslog(LOG_ERR, "AI-Service-Framework: close gpu top error");

    return ret;
}

static float get_gpu_occupation(Policy &policy) {

    std::string gpu_status_file = "/tmp/gpu_status.txt";
    if (access(gpu_status_file.c_str(), 0) != 0) { // mode = 0: whether file exist
        syslog(LOG_ERR, "AI-Service-Framework: gpu status file doesn't exist! ");
        return -1;
    }
    if (policy.CheckSymbolic(gpu_status_file.c_str()) == -1) {
        syslog(LOG_ERR, "AI-Service-Framework: ERROR: gpu status file is a symlink. Refuse opening it!");
        return -1;
    }
    std::ifstream ifs(gpu_status_file.c_str(), std::fstream::in);
    if (!ifs.is_open()) {
        syslog(LOG_ERR, "AI-Service-Framework: Failed to open gpu status file: %s", gpu_status_file.c_str());
        return -1;
    }

    std::string line;
    ifs.seekg(0, ifs.end);
    int length = ifs.tellg();
    if (length < (GPU_STATUS_HEADER + MIN_LINE_LEN)) {
        std::cout << "less than one line! " << length << std::endl;
        return -1;
    }

    int index = -2;
    std::string first_item;
    int act, irq, irq_percentage;
    float res_percentage;
    while(length)
    {
        char c;
        ifs.seekg(index, ifs.end);
        ifs.get(c);
        //check '\n' from end to begin
        if(c == '\n')
        {
            //get the the last line when finding its corresponding beginning
            std::getline(ifs, line);

            if (line.size() >= MIN_LINE_LEN) {
               std::istringstream iss(line);
               iss >> first_item;
               if ((first_item != "Freq") && (first_item != "req")) {
                   iss >> act >> irq >> irq_percentage >> res_percentage;
                   break;
               } else {
                   length -= (MIN_LINE_LEN - 1);
                   index -= (MIN_LINE_LEN - 1);
               }
            }
        }
        length--;
        index--;
    }

    ifs.close();

    return res_percentage;

}

static int process_gpu_occupancy(float &gpu_occupancy,
                                 std::string &device,
                                 Policy &policy,
                                 std::chrono::steady_clock::time_point &time) {

    if (device == "GPU") {
        if (gpuTopRunning) {
            //read gpu status
            gpu_occupancy = get_gpu_occupation(policy);
            // reastart gpu_top if neccessory
            if (std::chrono::duration_cast<std::chrono::seconds>(time - check_gpu_start_time).count() > RESTART_GPU_TIME) {
		int ret = closeGpuTop();
		if (ret != -1) {
                    gpuTopRunning = false;
                }
            }
        }
        if (!gpuTopRunning) {
            //start gpu top
	    int ret = system("sudo intel_gpu_top -s 500 -o /tmp/gpu_status.txt &");
            if (ret != -1) {
                gpuTopRunning = true;
                check_gpu_start_time = time;
            } else {
                syslog(LOG_ERR, "AI-Service-Framework: start gpu top error");
	    }
        }
    } else if (gpuTopRunning) {
	int ret = closeGpuTop();
        if (ret != -1) {
            gpuTopRunning = false;
        }
    }

    return 0;
}

static int stateTransfer(uint64_t &pss, long &timeout, int &iter, int iter_limit) {

    uint64_t limit = timeout > 60000 ? ((timeout/10000) + 2) : 6;

    if (pss > limit) {
        timeout = gDefaultTimeout;
        iter = 0;
        std::cout << "pss:"<< pss << ",limit:" << limit << std::endl;
    } else if (iter == iter_limit){
        auto next_timeout = (timeout == gDefaultTimeout) ? (timeout << 1) : (timeout << 2);
        auto longest = (gDefaultTimeout << 10) + (gDefaultTimeout << 8); //==640s
        timeout = (next_timeout >= longest) ? longest : next_timeout;
        iter = 0;
        std::cout << "transfer to " << timeout << std::endl;
    } else {
        iter++;
    }

    return 0;
}

static int gotoSleep(int &iter, uint64_t pss, std::future<bool> &activity) {

    if (gTimeout == gDefaultTimeout)
        stateTransfer(pss, gTimeout, iter, 120);
    else if (gTimeout == 2 * gDefaultTimeout)
        stateTransfer(pss, gTimeout, iter, 29);
    else
        stateTransfer(pss, gTimeout, iter, 0);

    if (activity.wait_for(std::chrono::milliseconds(gTimeout)) == std::future_status::ready) {
        syslog(LOG_ERR, "AI-Service-Framework: by wakeup");
        gTimeout = gDefaultTimeout;
        iter = 0;
    }

    return 0;
}

static void wakeup(int signum) {
    if (gTimeout != gDefaultTimeout) {
        std::lock_guard<std::mutex> lck(gMtx);
        if (gProm)
            gProm->set_value(true);
    }
}

int main(int argc, char *argv[]) {

    int iteration = 0;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler );
    signal(SIGTSTP, signalHandler );
    signal(SIGHUP, signalHandler );
    signal(SIGUSR1, wakeup);

    sigset_t set, oldset;

    if (sigfillset(&set) < 0)
        syslog(LOG_ERR, "AI-Service-Framework: init signal set error!");
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGTERM);
    sigdelset(&set, SIGTSTP);
    sigdelset(&set, SIGHUP);
    sigdelset(&set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &set, &oldset) < 0)
        syslog(LOG_ERR, "AI-Service-Framework: mask signals error!");

    gpuTopRunning = false;

    std::string cfgFile = "/etc/policy_setting.cfg";
    Policy policy(cfgFile);
    struct CfgParams cfgParam = policy.GetCfgParameters();

    int shmid = policy.CreateShm(sizeof(struct ShareMemory));
    if (shmid < 0) {
        syslog(LOG_ERR, "AI-Service-Framework: Error: create share memory. Eixt daemon");
        return 0;    
    }

    struct ShareMemory* buf = reinterpret_cast<struct ShareMemory*>(shmat(shmid,NULL, 0));
    buf->cpuStatus.idle_occupation = 1;
    buf->gpuStatus = -1; //GPU initial status
    buf->pidOfDaemon = getpid();
    policy.SaveCfgParams(cfgParam, buf); //write cfg to share buffers;

    //wait 2s for fcgi threads ready
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::vector<int> fcgi_ids;
    auto flag = getFcgiIds(fcgi_ids);

    auto start_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point check_gpu_start_time = start_time;
    node_list.pre = node_list.cur = 0;

    gTimeout = gDefaultTimeout = cfgParam.SamplingInterval;

    while(true) {
        std::promise<bool> prom;
        std::future<bool> fut = prom.get_future();
        {
            std::lock_guard<std::mutex> lck(gMtx);
            gProm = &prom;
        }

        auto end_time = std::chrono::steady_clock::now();
	//check configuration file and fcgi threads every 5s
	if (std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() > CHECK_TIME) {
            #ifdef CHECK_CFG_FILE
            //check changes of configuration file
	    if (policy.IsCfgFileChanged(cfgFile)) {
		syslog(LOG_NOTICE, "AI-Service-Framework: config file changed");
		policy.ParseConfiguration(cfgFile.c_str(), cfgParam);
		//update share memory
		policy.SaveCfgParams(cfgParam, buf);
	    }
            #endif

	    //get fcgi thread ids
	    fcgi_ids.clear();
	    getFcgiIds(fcgi_ids);

            start_time = end_time;
	}
        // Get CPU status
        uint64_t total_pss = 0;
        if (process_cpu_occupancy(buf->cpuStatus, fcgi_ids, total_pss) == 0) {
        }
        // Get GPU status
        std::string inferDevice(buf->cfgInferDev);
        process_gpu_occupancy(buf->gpuStatus, inferDevice, policy, end_time);

        gotoSleep(iteration, total_pss, fut);
        {
            std::lock_guard<std::mutex> lck(gMtx);
            gProm = nullptr;
        }
    }

    syslog(LOG_INFO, "AI-Service-Framework: delete share memory");
    policy.DestroyShm(shmid);

    return 0;
}
