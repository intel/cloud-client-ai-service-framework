#include <stdlib.h>

#include "conf.h"

void __attribute__ ((constructor)) ccai_init(void);
void __attribute__ ((destructor)) ccai_fini(void);

void ccai_init(void) {
    const char* path = getenv("NSS_CCAI_CONF");
    conf_init(path ?: "/etc/hosts_ccai");
}
void ccai_fini(void) {
    conf_fini();
}
