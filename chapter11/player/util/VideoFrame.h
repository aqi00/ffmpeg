#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

#define VideoFramePtr std::shared_ptr<VideoFrame>

class VideoFrame
{
public:
    VideoFrame();
    ~VideoFrame();

    void initBuffer(const int width, const int height); // 初始化图像缓存
    void setYUVbuf(const uint8_t *buf); // 设置YUV缓存数据
    void setRGBbuf(const uint8_t *buf); // 设置RGB缓存数据

    int width() {
        return mWidth;
    }
    int height() {
        return mHeight;
    }
    uint8_t * yuvBuffer() {
        return mYuv420Buffer;
    }
    uint8_t * rgbBuffer() {
        return mRgbBuffer;
    }

protected:
    uint8_t *mYuv420Buffer = NULL; // YUV缓存数据的地址
    uint8_t *mRgbBuffer = NULL; // RGB缓存数据的地址
    int mWidth; // 画面宽度
    int mHeight; // 画面高度
};

#endif // VIDEOFRAME_H
