#include <iostream> // C++使用iostream代替stdio.h

// 因为FFmpeg源码使用C语言编写，所以在C++代码中调用FFmpeg的话，要通过标记“extern "C"{……}”把FFmpeg的头文件包含进来
extern "C"
{
#include <libavutil/avutil.h>
}

int main(int argc, char** argv) {
    av_log(NULL, AV_LOG_INFO, "Hello World\n");
    return 0;
}