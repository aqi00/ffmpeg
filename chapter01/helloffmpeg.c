#include <stdio.h>
#include <libavutil/avutil.h>

int main(int argc, char** argv) {
    //av_log_set_level(AV_LOG_TRACE);
    av_log(NULL, AV_LOG_INFO, "Hello World\n");
    return 0;
}