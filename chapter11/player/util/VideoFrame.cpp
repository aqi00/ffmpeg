#include "VideoFrame.h"

// 构造方法
VideoFrame::VideoFrame()
{
    mYuv420Buffer = NULL;
    mRgbBuffer = NULL;
}

// 析构方法
VideoFrame::~VideoFrame()
{
    if (mYuv420Buffer != NULL)
    {
        free(mYuv420Buffer);
        mYuv420Buffer = NULL;
    }
    if (mRgbBuffer != NULL)
    {
        free(mRgbBuffer);
        mRgbBuffer = NULL;
    }
}

// 初始化图像缓存
void VideoFrame::initBuffer(const int width, const int height)
{
    if (mYuv420Buffer != NULL)
    {
        free(mYuv420Buffer);
        mYuv420Buffer = NULL;
    }
    if (mRgbBuffer != NULL)
    {
        free(mRgbBuffer);
        mRgbBuffer = NULL;
    }

    mWidth  = width;
    mHeight = height;
    mYuv420Buffer = (uint8_t*)malloc(width * height * 3 / 2);
    mRgbBuffer = (uint8_t*)malloc(width * height * 3);
}

// 设置YUV缓存数据
void VideoFrame::setYUVbuf(const uint8_t *buf)
{
    int Ysize = mWidth * mHeight;
    memcpy(mYuv420Buffer, buf, Ysize * 3 / 2);
}

// 设置RGB缓存数据
void VideoFrame::setRGBbuf(const uint8_t *buf)
{
    memcpy(mRgbBuffer, buf, mWidth * mHeight * 3);
}
