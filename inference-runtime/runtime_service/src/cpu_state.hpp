// Copyright (C) 2020 Intel Corporation
//

#pragma once

#define PROC_STAT_FILE                  "/proc/stat"
#define PROC_SELF_STAT_FILE             "/proc/self/stat"
#define PROC_SELF_STAT_START_POS        13
#define PROC_SELF_STAT_END_POS          17

struct proc_stat {
        uint64_t user;
        uint64_t nice;
        uint64_t system;
        uint64_t idle;
        uint64_t iowait;
        uint64_t irq;
        uint64_t softirq;
        uint64_t steal;
        uint64_t guest;
        uint64_t guest_nice;
};

#define BUSY_TIME(ps)           ((ps)->user + (ps)->nice + (ps)->system \
                                 + (ps)->irq + (ps)->softirq + (ps)->steal)
#define IDLE_TIME(ps)           ((ps)->idle + (ps)->iowait)

struct proc_self_stat {
        uint64_t utime;
        uint64_t stime;
        uint64_t cutime;
        uint64_t cstime;
};

#define SELF_TIME(pss)          ((pss)->utime + (pss)->stime + (pss)->cutime \
                                 + (pss)->cstime)

int read_proc_stat(struct proc_stat *ps);
int read_proc_self_stat(struct proc_self_stat *pss, int *pid);
