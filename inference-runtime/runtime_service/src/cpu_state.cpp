// Copyright (C) 2020 Intel Corporation
//

#include <unistd.h>
#include <syslog.h>
#include <iostream>
#include <chrono>
#include <cstring>
#include <string>

#include "cpu_state.hpp"

int read_proc_stat(struct proc_stat *ps) {

    char buf[256];
    FILE* file = fopen(PROC_STAT_FILE, "r");

    if (file == NULL) {
            syslog(LOG_ERR, "AI-Service-Framework: ERROR: Cannot open %s", PROC_STAT_FILE);
            return -1;
    }

    if(fgets(buf, sizeof(buf), file) != NULL) {
        sscanf(buf, "cpu  %16lu %16lu %16lu %16lu %16lu"
            "%16lu %16lu %16lu %16lu %16lu",
            &ps->user, &ps->nice, &ps->system, &ps->idle, &ps->iowait,
            &ps->irq, &ps->softirq, &ps->steal, &ps->guest, &ps->guest_nice);
    }

    fclose(file);

    return 0;
}

int read_proc_self_stat(struct proc_self_stat *pss, int *pid) {

    int i;
    char *pa, *pb;
    char buf[256];
    char proc_name[256];

    memset(proc_name, 0, 256);
    if (pid == NULL)
        snprintf(proc_name, 256, "%s", PROC_SELF_STAT_FILE);
    else
        snprintf(proc_name, 256, "/proc/%d/stat", *pid);

    FILE* file = fopen(proc_name, "r");
    if (file == NULL)
            return -1;

    if (fgets(buf, sizeof(buf), file) == NULL) {
        syslog(LOG_ERR, "AI-Service-Framework: ERROR: read proc");
        fclose(file);
        return -1;
    }

    fclose(file);

    pa = buf;
    for (i = 0; i < PROC_SELF_STAT_START_POS; i++) {
        pa = strchr(pa, ' ');
        if (pa == NULL)
            return -1;
        pa++;
    }

    pb = pa;
    for ( ; i < PROC_SELF_STAT_END_POS; i++) {
        pb = strchr(pb, ' ');
        if (pb == NULL)
            return -1;
        pb++;
    }

    *pb = '\0';

    sscanf(pa, "%16lu %16lu %16lu %16lu", &pss->utime, &pss->stime,
           &pss->cutime, &pss->cstime);

    return 0;
}

