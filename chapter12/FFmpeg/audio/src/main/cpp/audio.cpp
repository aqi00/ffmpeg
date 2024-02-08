#include <cstdio>
#include <cstring>
#include <jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "common.h"
#include "opensl.h"

// 由于FFmpeg库使用C语言实现，因此告诉编译器要遵循C语言的编译规则
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libswresample/swresample.h"
#include <libavutil/avutil.h>
};

#ifdef __cplusplus
extern "C" {
#endif

int is_stop = 0; // 是否停止播放。0 不停止；1 停止

JNIEXPORT void JNICALL
Java_com_example_audio_util_FFmpegUtil_playAudioByTrack(
        JNIEnv *env, jclass clazz, jstring audio_path)
{
    const char *audioPath = env->GetStringUTFChars(audio_path, 0);
    if (audioPath == NULL) {
        LOGE("audioPath is null");
        return;
    }
    LOGE("PlayAudio: %s", audioPath);
    AVFormatContext *fmt_ctx = avformat_alloc_context(); //分配封装器实例
    LOGI("Open audio file");
    // 打开音视频文件
    if (avformat_open_input(&fmt_ctx, audioPath, NULL, NULL) != 0) {
        LOGE("Cannot open audio file: %s\n", audioPath);
        return;
    }

    // 查找音视频文件中的流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Cannot find stream information.");
        return;
    }

    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index == -1) {
        LOGE("No audio stream found.");
        return;
    }

    AVCodecParameters *codec_para = fmt_ctx->streams[audio_index]->codecpar;
    LOGI("Find the decoder for the audio stream");
    // 查找音频解码器
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

    // 把音频流中的编解码参数复制给解码器的实例
    if (avcodec_parameters_to_context(decode_ctx, codec_para) < 0) {
        LOGE("Fill CodecContext failed.");
        return;
    }
    LOGE("decode_ctx bit_rate=%d", decode_ctx->bit_rate);
    LOGE("decode_ctx sample_fmt=%d", decode_ctx->sample_fmt);
    LOGE("decode_ctx sample_rate=%d", decode_ctx->sample_rate);
    LOGE("decode_ctx nb_channels=%d", decode_ctx->channels);

    LOGI("open Codec");
    // 打开解码器的实例
    if (avcodec_open2(decode_ctx, codec, NULL)) {
        LOGE("Open CodecContext failed.");
        return;
    }

    SwrContext *swr_ctx = NULL; // 音频采样器的实例
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的声道布局
    swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16,
                       decode_ctx->sample_rate,
                       &decode_ctx->ch_layout, decode_ctx->sample_fmt,
                       decode_ctx->sample_rate, 0, NULL
    );
    LOGE("swr_init");
    swr_init(swr_ctx); // 初始化音频采样器的实例

    // 原音频的通道数
    int channel_count = decode_ctx->ch_layout.nb_channels;
    // 单通道最大存放转码数据。所占字节 = 采样率*量化格式 / 8
    int out_size = 44100 * 16 / 8;
    uint8_t *out = (uint8_t *) (av_malloc(out_size));

    // 获取Java层clazz指向的类里面名叫create的方法编号
    jmethodID create_method_id = env->GetMethodID(clazz, "create", "(II)V");
    LOGE("begin CallVoidMethod");
    // 调用clazz指向的类里面对应编号的方法，该方法的返回类型为void，输入参数为采样频率和声道数量
    env->CallVoidMethod((jobject)clazz, create_method_id,
                        decode_ctx->sample_rate, out_ch_layout.nb_channels);
    LOGE("end CallVoidMethod");
    jmethodID play_method_id = env->GetMethodID(clazz, "play", "([BI)V");

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(fmt_ctx, packet) == 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要解码
            // 把未解压的数据包发给解码器实例
            int ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0) {
                LOGE("向解码器-发送数据");
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(decode_ctx, frame);
                if (ret == 0) {
                    LOGE("从解码器-接收数据");
                    // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
                    swr_convert(swr_ctx, &out, out_size,
                                (const uint8_t **) (frame->data), frame->nb_samples);
                    // 获取采样缓冲区的真实大小
                    int size = av_samples_get_buffer_size(NULL, channel_count,
                                                          frame->nb_samples,
                                                          AV_SAMPLE_FMT_S16, 1);
                    LOGE("out_size=%d, size=%d", out_size, size);
                    // 分配指定大小的Java字节数组
                    jbyteArray array = env->NewByteArray(size);
                    // 把音频缓冲区的数据复制到ava字节数组
                    env->SetByteArrayRegion(array, 0, size, (const jbyte *) (out));
                    // 调用clazz指向的类里面对应编号的方法，该方法的返回类型为void，输入参数为音频数据的字节数组和数据大小
                    env->CallVoidMethod((jobject)clazz, play_method_id, array, size);
                    // 回收指定的Java字节数组
                    env->DeleteLocalRef(array);
                    if (is_stop) { // 是否停止播放
                       break;
                    }
                }
            }
        }
//        av_packet_free(&packet); // 这句会导致程序挂掉
    }

    LOGI("release memory");
    // 释放各类资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    swr_free(&swr_ctx); // 释放音频采样器的实例
    avcodec_close(decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&decode_ctx); // 释放音频解码器的实例
    avformat_free_context(fmt_ctx); // 关闭音视频文件
    env->ReleaseStringUTFChars(audio_path, audioPath);
}


OpenslHelper helper;
uint8_t *out;
int buff_size;
AVFormatContext *fmt_ctx;
AVCodecContext *decode_ctx;
int audio_index;
AVPacket *packet;
AVFrame *frame;
SwrContext *swr_ctx;
int channel_count;
int out_size;

void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *pContext);

// 音频解码PCM，OpenSL播放
JNIEXPORT void JNICALL
Java_com_example_audio_util_FFmpegUtil_playAudioByOpenSL(
        JNIEnv *env, jclass clazz, jstring audio_path)
{
    const char *audioPath = env->GetStringUTFChars(audio_path, 0);
    if (audioPath == NULL) {
        LOGE("audioPath is null");
        return;
    }
    LOGE("PlayAudio: %s", audioPath);
    LOGI("Open audio file");
    // 打开音视频文件
    if (avformat_open_input(&fmt_ctx, audioPath, NULL, NULL) != 0) {
        LOGE("Cannot open audio file: %s\n", audioPath);
        return;
    }

    // 查找音视频文件中的流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Cannot find stream information.");
        return;
    }

    // 找到音频流的索引
    audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index == -1) {
        LOGE("No audio stream found.");
        return;
    }

    AVCodecParameters *codec_para = fmt_ctx->streams[audio_index]->codecpar;
    LOGI("Find the decoder for the audio stream");
    // 查找音频解码器
    AVCodec *codec = (AVCodec*) avcodec_find_decoder(codec_para->codec_id);
    if (codec == NULL) {
        LOGE("Codec not found.");
        return;
    }

    // 分配解码器的实例
    decode_ctx = avcodec_alloc_context3(codec);
    if (decode_ctx == NULL) {
        LOGE("CodecContext not found.");
        return;
    }

    // 把音频流中的编解码参数复制给解码器的实例
    if (avcodec_parameters_to_context(decode_ctx, codec_para) < 0) {
        LOGE("Fill CodecContext failed.");
        return;
    }
    LOGE("decode_ctx bit_rate=%d", decode_ctx->bit_rate);
    LOGE("decode_ctx sample_fmt=%d", decode_ctx->sample_fmt);
    LOGE("decode_ctx sample_rate=%d", decode_ctx->sample_rate);
    LOGE("decode_ctx nb_channels=%d", decode_ctx->channels);

    LOGI("open Codec");
    // 打开解码器的实例
    if (avcodec_open2(decode_ctx, codec, NULL)) {
        LOGE("Open CodecContext failed.");
        return;
    }

    swr_ctx = NULL; // 音频采样器的实例
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的声道布局
    swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16,
                        decode_ctx->sample_rate,
                        &decode_ctx->ch_layout, decode_ctx->sample_fmt,
                        decode_ctx->sample_rate, 0, NULL
    );
    LOGE("swr_init");
    swr_init(swr_ctx); // 初始化音频采样器的实例

    // 原音频的通道数
    channel_count = decode_ctx->ch_layout.nb_channels;
    // 单通道最大存放转码数据。所占字节 = 采样率*量化格式 / 8
    out_size = 44100 * 16 / 8;
    out = (uint8_t *) (av_malloc(out_size));

    SLresult result = helper.createEngine(); // 创建OpenSL引擎
    if (!helper.isSuccess(result)) { // 创建引擎失败
        LOGE("create engine error");
        return;
    }
    result = helper.createMix(); // 创建混音器
    if (!helper.isSuccess(result)) { // 创建混音器失败
        LOGE("create mix error");
        return;
    }
    // 创建播放器
    result = helper.createPlayer(channel_count, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16,
                                 SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
    if (!helper.isSuccess(result)) { // 创建播放器失败
        LOGE("create player error");
        return;
    }

    packet = av_packet_alloc(); // 分配一个数据包
    frame = av_frame_alloc(); // 分配一个数据帧
    helper.registerCallback(playerCallback); // 注册回调入口
    LOGE("end registerCallback");
    helper.play(); // 开始播放
    LOGE("end play false=%d, true=%d", false, true);
    playerCallback(helper.bufferQueueItf, NULL); // 开始播放首帧音频
    LOGE("end playerCallback");

    env->ReleaseStringUTFChars(audio_path, audioPath);
}

void release() {
//    av_freep(out); // 这句会导致程序挂掉
    out = NULL;
    // 释放各类资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    swr_free(&swr_ctx); // 释放音频采样器的实例
    avcodec_close(decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&decode_ctx); // 释放音频解码器的实例
    avformat_free_context(fmt_ctx); // 关闭音视频文件
}

bool is_finished = false;
// 获取音频数据
void getAudioData(uint8_t **out, int *buff_size) {
    LOGE("begin getAudioData");
    if (out == NULL || buff_size == NULL) {
        return;
    }

    while (true) {
        int ret = av_read_frame(fmt_ctx, packet); // 轮询数据包
        if (ret != 0) {
            is_finished = true;
            break;
        }
        if (packet->stream_index == audio_index) { // 音频包需要解码
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0) {
                LOGE("向解码器-发送数据");
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(decode_ctx, frame);
                if (ret == 0) {
                    LOGE("从解码器-接收数据");
                    // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
                    swr_convert(swr_ctx, out, out_size,
                                (const uint8_t **) (frame->data), frame->nb_samples);
                    // 获取采样缓冲区的真实大小
                    *buff_size = av_samples_get_buffer_size(NULL, channel_count,
                                                            frame->nb_samples,
                                                            AV_SAMPLE_FMT_S16, 1);
                    LOGE("out_size=%d, size=%d", out_size, buff_size);
                } else {
                    is_finished = true;
                }
                break;
            }
        }
//        av_packet_free(&packet); // 这句会导致程序挂掉
    }
}

// 播放器会不断调用该函数，需要在此回调中持续向缓冲区填充数据
void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *pContext) {
    LOGE("playerCallback helper.playState=%d, SL_PLAYSTATE_PLAYING=%d, SL_PLAYSTATE_STOPPED=%d, is_finished=%d",
         helper.playState, SL_PLAYSTATE_PLAYING, SL_PLAYSTATE_STOPPED, is_finished);
    if (is_finished || helper.playState == SL_PLAYSTATE_STOPPED) {
        release(); // 释放各类资源
        LOGE("end release");
        helper.~OpenslHelper();
    } else if (helper.playState == SL_PLAYSTATE_PLAYING) {
        getAudioData(&out, &buff_size); // 获取音频数据
        if (out != NULL && buff_size != 0) {
            (*bq)->Enqueue(bq, out, (SLuint32) (buff_size));
        }
    }
}

JNIEXPORT void JNICALL
Java_com_example_audio_util_FFmpegUtil_stopPlayByOpenSL(JNIEnv *env, jclass clazz)
{
    helper.stop(); // 停止播放
}

JNIEXPORT void JNICALL
Java_com_example_audio_util_FFmpegUtil_stopPlayByTrack(JNIEnv *env, jclass clazz)
{
    is_stop = 1;
}

#ifdef __cplusplus
}
#endif