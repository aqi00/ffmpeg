#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

#include <QImage>
#include <QWidget>

#include "util/FunctionTransfer.h"
#include "util/VideoFrame.h"

// 由于FFmpeg库使用C语言实现，因此告诉编译器要遵循C语言的编译规则
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
};

class VideoView : public QWidget {
    Q_OBJECT
public:
    explicit VideoView(QWidget *parent = NULL);
    ~VideoView();

public slots:
    // 视频帧已被解码，准备展示到界面上
    void onFrameDecoded(VideoFramePtr videoFrame);

private:
    QImage *_image = NULL; // 图像控件
    QRect _rect; // 矩形框
    int count = 0;
    VideoFramePtr mVideoFrame; // 视频帧的指针
    void paintEvent(QPaintEvent *event) override;
    void freeImage(); // 释放图像资源
};

#endif // VIDEOVIEW_H
