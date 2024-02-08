#include <stdio.h>
#include <SDL.h>
// 引入SDL要增加下面的声明#undef main，否则编译会报错“undefined reference to `WinMain'”
#undef main

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavutil/avutil.h>
#ifdef __cplusplus
};
#endif

int number; // 声明一个整型变量

// 分线程的任务处理
int thread_work(void *arg) {
    int count = 0;
    while (++count < 10) {
        av_log(NULL, AV_LOG_INFO, "Thread begin deal , the number is %d\n", number);
        SDL_Delay(100); // 延迟若干时间，单位毫秒
        av_log(NULL, AV_LOG_INFO, "Thread end deal, the number is %d\n", number);
        SDL_Delay(100); // 延迟若干时间，单位毫秒
    }
    return 1; // 返回线程的结束标志
}

int main(int argc, char **argv) {
    // 创建SDL线程，指定任务处理函数，并返回线程编号
    SDL_Thread *sdl_thread = SDL_CreateThread(thread_work, "thread_work", NULL);
    if (!sdl_thread) {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    int count = 0;
    while (++count < 100) {
        number = count;
        SDL_Delay(30); // 延迟若干时间，单位毫秒
    }
    int finish_status; // 线程的结束标志
    SDL_WaitThread(sdl_thread, &finish_status); // 等待线程结束，结束标志在status字段返回
    av_log(NULL, AV_LOG_INFO, "sdl_thread finish_status=%d\n", finish_status);
    return 0;
}
