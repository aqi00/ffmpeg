#ifndef VIDEODECODER_H
#define VIDEODECODER_H

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
#ifdef __cplusplus
};
#endif

class VideoDecoder
{
public:
    VideoDecoder(int play_type, const char *video_path, VideoCallBack *callback);
    ~VideoDecoder();

    void start(); // 开始解码
    void stop(); // 停止解码
    void pause(); // 暂停解码
    void resume(); // 恢复解码
private:
    int m_play_type; // 播放类型。0为QImage方式，1为OpenGL方式
    const char *m_video_path; // 视频文件的路径
    VideoCallBack *m_callback; // 视频回调接口
    bool is_stop = false; // 是否停止解码
    bool is_pause = false; // 是否暂停解码
    uint8_t *m_rgb_buffer = NULL; // RGB数据缓存
    uint8_t *m_yuv_buffer = NULL; // YUV数据缓存

    int playVideo(); // 播放视频
    void displayImage(int width, int height); // 显示图像
    void sleep(long millisecond); // 睡眠若干毫秒
};

#endif // VIDEODECODER_H
