#ifndef VideoPlayer_H
#define VideoPlayer_H

#include <list>
#include <SDL.h>

#include "util//VideoFrame.h"
#include "util/videocallback.h"

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif

class VideoPlayer
{
public:
    VideoPlayer(const char *video_path, VideoCallBack *callback);
    ~VideoPlayer();

    const char *start(); // 开始播放
    void stop(); // 停止播放
    void pause(); // 暂停播放
    void resume(); // 恢复播放
    void reset(); // 重置
    void clear(); // 清除
private:
    const char *m_video_path; // 视频文件的路径
    VideoCallBack *m_callback; // 视频回调接口

    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例

    int playVideo(); // 播放视频
    void displayImage(int width, int height); // 显示图像

    // 回调函数，在获取音频数据后调用
    void static fill_audio(void *para, uint8_t *stream, int len);
    int static thread_work_audio(void *arg); // 音频分线程的任务处理
    int static thread_work_video(void *arg); // 视频分线程的任务处理
    int open_input_file(const char *src_name); // 打开输入文件
    int init_sws_context(); // 初始化图像转换器的实例
    int prepare_video(); // 准备视频相关资源
    int prepare_audio(); // 准备音频相关资源
    int play_video_frame(); // 播放视频画面
    void release(); // 释放资源
};

#endif // VideoPlayer_H
