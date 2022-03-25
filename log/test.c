#include "ccai_log.h"

/* control variables (may be overrided by environment variables)
Note:
Executable has to be linked with '-Wl,--dynamic-list=dynamic.list'
(or '-Wl,--dynamic-list-data' for simple) to expose these variables.
const char* ccai_log_ident     = "log.test";
const char* ccai_log_server    = "udp://127.0.0.1";
const char* ccai_log_stderr    = "y";
const char* ccai_log_threshold = "info";
*/

int main(int argc, char* argv[]) {
    CCAI_ERROR("%s", "test error log\n");
    CCAI_WARN("%s", "test warning log\n");
    CCAI_INFO("%s", "test info log\n");
    CCAI_DEBUG("%s", "test debug log\n");
    return 0;
}
