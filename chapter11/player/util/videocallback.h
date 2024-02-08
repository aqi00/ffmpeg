#ifndef VIDEOCALLBACK_H
#define VIDEOCALLBACK_H

#include "VideoFrame.h"

// 由于FFmpeg库使用C语言实现，因此告诉编译器要遵循C语言的编译规则
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
};

class VideoCallBack
{
public:
    ~VideoCallBack();

    // 播放视频，此函数不宜做耗时操作，否则会影响播放的流畅性。
    virtual void onDisplayVideo(VideoFramePtr videoFrame) = 0;
    // 停止播放。通知界面修改按钮状态
    virtual void onStopPlay() = 0;
};

#endif // VIDEOCALLBACK_H
