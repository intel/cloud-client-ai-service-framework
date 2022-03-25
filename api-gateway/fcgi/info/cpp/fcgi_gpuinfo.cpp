// Copyright (C) 2020 Intel Corporation


// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi
#include <syslog.h>
#include <string>
#include <stdio.h>
#include <string.h>
#include <fcgiapp.h>
#include <fcgio.h>
#include <fcgi_stdio.h>

#if USE_LIBPCI
extern "C" {
#include <pci/pci.h>
}
#endif

#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

const char* freq_types[] = {"max", "min", "cur", "RP0", "RP1", "RPn", NULL };

size_t readstring(const char* path, char* buf, size_t size) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1UL;
    size_t len = fread(buf, 1, size - 1, fp);
    buf[len] = 0;
    fclose(fp);
    return len;
}

int main(int argc, char **argv) {
    char buf[1024];
    openlog("testfastcgi", LOG_CONS|LOG_NDELAY, LOG_USER);

    char vendor_id[8], vendor_str[256];
    char device_id[8], device_str[256];
    readstring("/sys/class/drm/card0/device/vendor", vendor_id, sizeof(vendor_id));
    readstring("/sys/class/drm/card0/device/device", device_id, sizeof(device_id));

#if USE_LIBPCI
    struct pci_access *pacc = pci_alloc();
    pci_init(pacc);

    uint16_t vid = strtol(vendor_id, NULL, 16);
    pci_lookup_name(pacc, vendor_str, sizeof(vendor_str), PCI_LOOKUP_VENDOR, vid);

    uint16_t did = strtol(device_id, NULL, 16);
    pci_lookup_name(pacc, device_str, sizeof(device_str), PCI_LOOKUP_DEVICE, vid, did);
    pci_cleanup(pacc);
#else
    sprintf(vendor_str, "%.6s", vendor_id);
    sprintf(device_str, "%.6s", device_id);
    snprintf(buf, sizeof(buf), "lspci -vmm -d %s:%s", vendor_str, device_str);
    FILE *fp = popen(buf, "r");
    if (fp) {
        while (fgets(buf, sizeof(buf), fp)) {
            size_t l = strlen(buf) - 1;
            if (buf[l] == '\n')
                buf[l] = '\0';
            if (!strncmp(buf, "Vendor:", 7))
                strncpy(vendor_str, buf + 8, sizeof(vendor_str));
            else if (!strncmp(buf, "Device:", 7))
                strncpy(device_str, buf + 8, sizeof(device_str));
        }
        pclose(fp);
    }
#endif

    int err = FCGX_Init(); /* call before Accept in multithreaded apps */
    if (err) {
        syslog (LOG_INFO, "FCGX_Init failed: %d", err);
	return 1;
    }
    FCGX_Request cgi;
    err = FCGX_InitRequest(&cgi, LISTENSOCK_FILENO, LISTENSOCK_FLAGS);
    if (err) {
        syslog(LOG_INFO, "FCGX_InitRequest failed: %d", err);
	return 2;
    }
    while (1) {
        err = FCGX_Accept_r(&cgi);
        if (err) {
	    syslog(LOG_INFO, "FCGX_Accept_r stopped: %d", err);
	    break;
	}

        std::string result("Status: 200 OK\r\nContent-Type: text/plain\r\n"
                           "X-Content-Type-Options: nosniff\r\nX-frame-options: deny\r\n\r\n");
        result += std::string("Vendor: ") + vendor_str + "\n";
        result += std::string("Device: ") + device_str + "\n";

        for (int i = 0; freq_types[i]; ++i) {
            snprintf(buf, sizeof(buf), "/sys/class/drm/card0/gt_%s_freq_mhz", freq_types[i]);
            if (readstring(buf, buf, sizeof(buf)) != -1UL)
                result += std::string(freq_types[i]) + ": " + buf;
        }

        FCGX_PutStr(result.c_str(), result.length(), cgi.out);
    }

    return 0;
}
