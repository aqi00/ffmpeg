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

// 分线程的任务处理
int thread_work(void *arg) {
    int loop_count = *(int *)arg; // 取出线程的输入参数
    int i = 0;
    while (++i < loop_count+1) {
        av_log(NULL, AV_LOG_INFO, "The thread work on %d seconds\n", i);
        SDL_Delay(1000); // 延迟若干时间，单位毫秒
    }
    return 1; // 返回线程的结束标志
}

int main(int argc, char **argv) {
    int loop_count = 3;
    if (argc > 2) {
        loop_count = atoi(argv[2]);
    }
    // 创建SDL线程，指定任务处理函数，并返回线程编号
    SDL_Thread *sdl_thread = SDL_CreateThread(thread_work, "thread_work", &loop_count);
    if (!sdl_thread) {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    // 线程分离之后，即使调用SDL_WaitThread函数也不会等待线程结束
//    SDL_DetachThread(sdl_thread);
    int finish_status; // 线程的结束标志
    SDL_WaitThread(sdl_thread, &finish_status); // 等待线程结束，结束标志在status字段返回
    av_log(NULL, AV_LOG_INFO, "sdl_thread finish_status=%d\n", finish_status);
    return 0;
}
