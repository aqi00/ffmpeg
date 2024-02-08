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

SDL_mutex *sdl_lock = NULL; // 声明一个互斥锁，防止线程间同时操作某个变量
SDL_cond *sdl_signal = NULL; // 声明一个信号量（条件变量）
int number; // 声明一个整型变量

// 分线程的任务处理
int thread_work1(void *arg) {
    int count = 0;
    SDL_LockMutex(sdl_lock); // 对互斥锁加锁
    while (++count < 10) {
        av_log(NULL, AV_LOG_INFO, "First thread begin deal, the number is %d\n", number);
        SDL_Delay(100); // 延迟若干时间，单位毫秒
        av_log(NULL, AV_LOG_INFO, "First thread end deal, the number is %d\n", number);
        // 释放锁资源（解锁），并等待响应信号。收到响应信号之后，会重新加锁
        SDL_CondWait(sdl_signal, sdl_lock);
    }
    SDL_UnlockMutex(sdl_lock); // 对互斥锁解锁
    return 1; // 返回线程的结束标志
}

// 分线程的任务处理
int thread_work2(void *arg) {
    int count = 0;
    SDL_LockMutex(sdl_lock); // 对互斥锁加锁
    while (++count < 10) {
        av_log(NULL, AV_LOG_INFO, "Second thread begin deal, the number is %d\n", number);
        SDL_Delay(50); // 延迟若干时间，单位毫秒
        av_log(NULL, AV_LOG_INFO, "Second thread end deal, the number is %d\n", number);
        // 释放锁资源（解锁），并等待响应信号。收到响应信号之后，会重新加锁
        SDL_CondWait(sdl_signal, sdl_lock);
    }
    SDL_UnlockMutex(sdl_lock); // 对互斥锁解锁
    return 1; // 返回线程的结束标志
}

int main(int argc, char **argv) {
    sdl_lock = SDL_CreateMutex(); // 创建互斥锁
    sdl_signal = SDL_CreateCond(); // 创建信号量（条件变量）
    // 创建SDL线程，指定任务处理函数，并返回线程编号
    SDL_Thread *sdl_thread1 = SDL_CreateThread(thread_work1, "thread_work1", NULL);
    if (!sdl_thread1) {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    SDL_Delay(10); // 延迟若干时间，单位毫秒
    // 创建SDL线程，指定任务处理函数，并返回线程编号
    SDL_Thread *sdl_thread2 = SDL_CreateThread(thread_work2, "thread_work2", NULL);
    if (!sdl_thread2) {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    SDL_Delay(10); // 延迟若干时间，单位毫秒
    int count = 0;
    while (++count < 50) {
        SDL_LockMutex(sdl_lock); // 对互斥锁加锁
        number = count;
        SDL_Delay(10); // 延迟若干时间，单位毫秒
        SDL_CondSignal(sdl_signal); // 发出响应信号
        SDL_UnlockMutex(sdl_lock); // 对互斥锁解锁
        SDL_Delay(10); // 延迟若干时间，单位毫秒
    }
    int finish_status; // 线程的结束标志
    SDL_WaitThread(sdl_thread1, &finish_status); // 等待线程结束，结束标志在status字段返回
    av_log(NULL, AV_LOG_INFO, "sdl_thread1 finish_status=%d\n", finish_status);
    SDL_WaitThread(sdl_thread2, &finish_status); // 等待线程结束，结束标志在status字段返回
    av_log(NULL, AV_LOG_INFO, "sdl_thread2 finish_status=%d\n", finish_status);
    SDL_DestroyCond(sdl_signal); // 销毁信号量（条件变量）
    SDL_DestroyMutex(sdl_lock); // 销毁互斥锁
    return 0;
}
