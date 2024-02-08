#include <cstdio>
#include <cstring>
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "common.h"
#include "opengl.h"

// 由于FFmpeg库使用C语言实现，因此告诉编译器要遵循C语言的编译规则
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
};

#ifdef __cplusplus
extern "C" {
#endif

// 开始播放视频
JNIEXPORT void JNICALL
Java_com_example_video_util_FFmpegUtil_playVideoByNative(
        JNIEnv *env, jclass clazz, jstring video_path, jobject surface)
{
    const char *videoPath = env->GetStringUTFChars(video_path, 0);
    LOGE("PlayVideo: %s", videoPath);
    if (videoPath == NULL) {
        LOGE("videoPath is null");
        return;
    }
    is_stop = 0;

    AVFormatContext *fmt_ctx = avformat_alloc_context(); // 分配封装器实例
    LOGI("Open video file");
    // 打开音视频文件
    if (avformat_open_input(&fmt_ctx, videoPath, NULL, NULL) != 0) {
        LOGE("Cannot open video file: %s\n", videoPath);
        return;
    }

    LOGI("Retrieve stream information");
    // 查找音视频文件中的流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Cannot find stream information.");
        return;
    }

    LOGI("Find video stream");
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index == -1) {
        LOGE("No video stream found.");
        return;
    }
    int fps = av_q2d(fmt_ctx->streams[video_index]->r_frame_rate); // 帧率
    int interval = round(1000 * 1000 / fps); // 根据帧率计算每帧之间的播放间隔
    int interval2 = fmt_ctx->duration / fmt_ctx->streams[video_index]->nb_frames;
    LOGE("fps=%d, interval=%d, duration=%d, nb_frames=%d, interval2=%d", fps, interval,
         fmt_ctx->duration, fmt_ctx->streams[video_index]->nb_frames, interval2);

    AVCodecParameters *codec_para = fmt_ctx->streams[video_index]->codecpar;
    LOGI("Find the decoder for the video stream");
    // 查找视频解码器
    AVCodec *codec = (AVCodec*) avcodec_find_decoder(codec_para->codec_id);
    if (codec == NULL) {
        LOGE("Codec not found.");
        return;
    }

    // 分配解码器的实例
    AVCodecContext *decode_ctx = avcodec_alloc_context3(codec);
    if (decode_ctx == NULL) {
        LOGE("CodecContext not found.");
        return;
    }

    // 把视频流中的编解码参数复制给解码器的实例
    if (avcodec_parameters_to_context(decode_ctx, codec_para) < 0) {
        LOGE("Fill CodecContext failed.");
        return;
    }

    LOGI("open Codec");
    // 打开解码器的实例
    if (avcodec_open2(decode_ctx, codec, NULL)) {
        LOGE("Open CodecContext failed.");
        return;
    }

    enum AVPixelFormat dest_format = AV_PIX_FMT_RGBA;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    AVFrame *render_frame = av_frame_alloc(); // 分配一个渲染帧

    LOGI("Determine required buffer size and allocate buffer");
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(render_frame->data, render_frame->linesize,
                   decode_ctx->width, decode_ctx->height, dest_format, 1);

    LOGI("init SwsContext");
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    struct SwsContext *swsContext = sws_getContext(
            decode_ctx->width, decode_ctx->height, decode_ctx->pix_fmt,
            decode_ctx->width, decode_ctx->height, dest_format,
            SWS_BILINEAR, NULL, NULL, NULL);
    if (swsContext == NULL) {
        LOGE("Init SwsContext failed.");
        return;
    }

    LOGI("native window");
    // 从表面对象获取原生窗口
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    LOGI("set native window");
    // 设置原生窗口的缓冲区大小
    ANativeWindow_setBuffersGeometry(nativeWindow, decode_ctx->width,
                                     decode_ctx->height, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer; // 声明窗口缓存结构

    LOGI("read frame");
    long play_time = av_gettime(); // 各帧的约定播放时间点
    long now_time = av_gettime(); // 当前时间点
    while (av_read_frame(fmt_ctx, packet) == 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要解码
            // 把未解压的数据包发给解码器实例
            int ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0) {
                LOGE("向解码器-发送数据");
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(decode_ctx, frame);
                if (ret == 0) {
                    LOGE("从解码器-接收数据");
                    // 锁定窗口对象和窗口缓存
                    ANativeWindow_lock(nativeWindow, &windowBuffer, NULL);
                    // 开始转换图像格式
                    sws_scale(swsContext, (uint8_t const *const *) frame->data,
                              frame->linesize, 0, decode_ctx->height,
                              render_frame->data, render_frame->linesize);
                    uint8_t *dst = (uint8_t *) windowBuffer.bits;
                    uint8_t *src = (render_frame->data[0]);
                    int dstStride = windowBuffer.stride * 4;
                    int srcStride = render_frame->linesize[0];
                    // 由于原生窗口的每行大小与数据帧的每行大小不同，因此需要逐行复制
                    for (int i = 0; i < decode_ctx->height; i++) {
                        memcpy(dst + i * dstStride, src + i * srcStride, srcStride);
                    }
                    ANativeWindow_unlockAndPost(nativeWindow); // 解锁窗口对象

                    now_time = av_gettime();
                    play_time += interval; // 下一帧的约定播放时间点
                    long temp_interval = play_time-now_time;
                    temp_interval = (temp_interval < 0) ? 0 : temp_interval;
                    LOGE("interval=%lld, temp_interval=%lld", interval, temp_interval);
                    av_usleep(temp_interval); // 睡眠若干微秒
                    if (is_stop) { // 是否停止播放
                        break;
                    }
                } else {
                    LOGE("从解码器-接收-数据失败：%d", ret);
                }
            } else {
                LOGE("向解码器-发送-数据失败：%d", ret);
            }
        }
        av_packet_unref(packet);
    }

    LOGI("release memory");
    // 释放各类资源
    ANativeWindow_release(nativeWindow); // 释放原生窗口
    av_frame_free(&frame);
    av_frame_free(&render_frame);
    av_packet_free(&packet);
    avcodec_close(decode_ctx);
    avcodec_free_context(&decode_ctx);
    avformat_close_input(&fmt_ctx);
    avformat_free_context(fmt_ctx);
    env->ReleaseStringUTFChars(video_path, videoPath);
}

JNIEXPORT void JNICALL
Java_com_example_video_util_FFmpegUtil_playVideoByOpenGL(
        JNIEnv *env, jclass clazz, jstring video_path, jobject surface)
{
    play_video(env, clazz, video_path, surface);
}

// 停止播放视频
JNIEXPORT void JNICALL
Java_com_example_video_util_FFmpegUtil_stopPlay(
        JNIEnv *env, jclass clazz)
{
    is_stop = 1;
}

#ifdef __cplusplus
}
#endif