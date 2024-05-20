#include "videodecoder.h"
#include <QDebug>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

// 构造方法
VideoDecoder::VideoDecoder(int play_type, const char *video_path, VideoCallBack *callback)
{
    qInfo() << "VideoDecoder play_type=" << play_type << '\n';
    m_play_type = play_type;
    m_video_path = video_path;
    m_callback = callback;
}

// 析构方法
VideoDecoder::~VideoDecoder()
{
}

// 开始解码
void VideoDecoder::start()
{
    is_stop = false;
    // 开启分线程播放视频。detach表示分离该线程
    std::thread([this](){
        int ret = playVideo(); // 播放视频
        qInfo() << "play result: " << ret << '\n';
    }).detach();
}

// 停止解码
void VideoDecoder::stop()
{
    is_pause = false;
    is_stop = true;
}

// 暂停解码
void VideoDecoder::pause()
{
    is_pause = true;
}

// 恢复解码
void VideoDecoder::resume()
{
    is_pause = false;
}

// 播放视频
int VideoDecoder::playVideo()
{
    qInfo() << "playVideo " << m_video_path << '\n';
    AVFormatContext *in_fmt_ctx = avformat_alloc_context(); // 输入文件的封装器实例
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, m_video_path, NULL, NULL);
    if (ret < 0) {
        qCritical() << "Can't open file " << m_video_path << '\n';
        return -1;
    }
    qInfo() << "Success open input_file " << m_video_path << '\n';
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        qCritical() << "Can't find stream information" << '\n';
        return -1;
    }
    AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例
    AVStream *src_video = NULL;
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            qCritical() << "video_codec not found" << '\n';
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            qCritical() << "video_decode_ctx is null" << '\n';
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            qCritical() << "Can't open video_decode_ctx" << '\n';
            return -1;
        }
    } else {
        qCritical() << "Can't find video stream" << '\n';
        return -1;
    }

    int width = video_decode_ctx->width;
    int height = video_decode_ctx->height;
    enum AVPixelFormat rgb_format = AV_PIX_FMT_RGB24; // QImage支持RGB空间
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    SwsContext *m_rgb_sws = sws_getContext(width, height, video_decode_ctx->pix_fmt,
        width, height, rgb_format, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (m_rgb_sws == NULL) {
        qCritical() << "rgb swsContext is null" << '\n';
        return -1;
    }
    AVFrame *m_rgb_frame = av_frame_alloc(); // 分配一个RGB数据帧
    // 分配缓冲区空间，用于存放转换后的图像数据
    int rgb_size = av_image_get_buffer_size(rgb_format, width, height, 1);
    m_rgb_buffer = (uint8_t *)av_malloc((size_t)rgb_size * sizeof(uint8_t));
    // 将数据帧与缓冲区关联
    av_image_fill_arrays(m_rgb_frame->data, m_rgb_frame->linesize, m_rgb_buffer,
                         rgb_format, width, height, 1);

    enum AVPixelFormat yuv_format = AV_PIX_FMT_YUV420P; // OpenGL支持YUV空间
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    SwsContext *m_yuv_sws = sws_getContext(width, height, video_decode_ctx->pix_fmt,
        width, height, yuv_format, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (m_yuv_sws == NULL) {
        qCritical() << "yuv swsContext is null" << '\n';
        return -1;
    }
    AVFrame *m_yuv_frame = av_frame_alloc(); // 分配一个YUV数据帧
    // 分配缓冲区空间，用于存放转换后的图像数据
    int yuv_size = av_image_get_buffer_size(yuv_format, width, height, 1);
    m_yuv_buffer = (uint8_t *)av_malloc((size_t)yuv_size * sizeof(uint8_t));
    // 将数据帧与缓冲区关联
    av_image_fill_arrays(m_yuv_frame->data, m_yuv_frame->linesize, m_yuv_buffer,
                         yuv_format, width, height, 1);

    int fps = av_q2d(src_video->avg_frame_rate); // 帧率
    int interval = round(1000 / fps); // 根据帧率计算每帧之间的播放间隔
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要解码
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(video_decode_ctx, packet);
            if (ret == 0) {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(video_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                } else if (ret < 0) {
                    qCritical() << "decode frame occur error " << ret << '\n';
                    return -1;
                }
                // 转换器开始处理图像数据，把视频帧转为RGB图像
                sws_scale(m_rgb_sws, (uint8_t const * const *) frame->data, frame->linesize,
                          0, frame->height, m_rgb_frame->data, m_rgb_frame->linesize);
                // 转换器开始处理图像数据，把视频帧转为YUV图像
                sws_scale(m_yuv_sws, (uint8_t const * const *) frame->data, frame->linesize,
                          0, frame->height, m_yuv_frame->data, m_yuv_frame->linesize);
                displayImage(width, height); // 显示图像
                qInfo() << "interval=" << interval;
                sleep(interval); // 延迟若干时间，单位毫秒
                while (is_pause) { // 如果暂停播放，就持续休眠；直到恢复播放才继续解码
                    sleep(20); // 休眠20毫秒
                    if (is_stop) { // 暂停期间如果停止播放，就结束暂停
                        break;
                    }
                }
                if (is_stop) { // 如果停止播放，就跳出循环结束解码
                    break;
                }
            } else {
                qCritical() << "send packet occur error " << ret << '\n';
                return -1;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    qInfo() << "Success play video file" << '\n';

    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    av_frame_free(&m_rgb_frame); // 释放数据帧资源
    av_frame_free(&m_yuv_frame); // 释放数据帧资源
    sws_freeContext(m_rgb_sws); // 释放图像转换器的实例
    sws_freeContext(m_yuv_sws); // 释放图像转换器的实例
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    qInfo() << "Quit Play" << '\n';
    stop();
    m_callback->onStopPlay(); // 通知界面修改按钮状态
    return 0;
}

// 显示图像
void VideoDecoder::displayImage(int width, int height) {
    VideoFramePtr videoFrame = std::make_shared<VideoFrame>();
    VideoFrame *ptr = videoFrame.get();
    ptr->initBuffer(width, height); // 初始化图像缓存
    if (m_play_type == 0) { // 使用QImage方式
        ptr->setRGBbuf(m_rgb_buffer); // 设置RGB缓存数据
    } else { // 使用OpenGL方式
        ptr->setYUVbuf(m_yuv_buffer); // 设置YUV缓存数据
    }
    // 通知回调接口展示该帧视频的图像
    m_callback->onDisplayVideo(videoFrame);
}

// 睡眠若干毫秒
void VideoDecoder::sleep(long millisecond) {
#ifdef _WIN32
    _sleep(millisecond);
#else
    usleep(millisecond * 1000);
#endif
}
