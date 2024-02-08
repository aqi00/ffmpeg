#include <stdio.h>

//libavutil/common.h要求定义，否则会报错：error missing -D__STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavutil/avutil.h>
#ifdef __cplusplus
};
#endif

int main(int argc, char** argv) {
    av_log(NULL, AV_LOG_INFO, "Hello World\n");
    return 0;
}