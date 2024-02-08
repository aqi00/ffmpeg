#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif

AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
AVCodecContext *audio_decode_ctx = NULL; // 音频解码器的实例
int audio_index = -1; // 音频流的索引
AVCodecContext *audio_encode_ctx = NULL; // MP3编码器的实例

// 打开输入文件
int open_input_file(const char *src_name) {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到音频流的索引
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0) {
        AVStream *src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec*) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "audio_decode_ctx is null\n");
            return -1;
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        ret = avcodec_open2(audio_decode_ctx, audio_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find audio stream.\n");
        return -1;
    }
    return 0;
}

// 初始化MP3编码器的实例
int init_audio_encoder(int nb_channels) {
    // 查找MP3编码器
    AVCodec *audio_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!audio_codec) {
        av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
        return -1;
    }
    const enum AVSampleFormat *p = audio_codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) { // 使用AV_SAMPLE_FMT_NONE作为结束符
        av_log(NULL, AV_LOG_INFO, "audio_codec support format %d\n", *p);
        p++;
    }
    // 获取编解码器上下文信息
    audio_encode_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_encode_ctx) {
        av_log(NULL, AV_LOG_ERROR, "audio_encode_ctx is null\n");
        return -1;
    }
    // lame库支持AV_SAMPLE_FMT_S16P、AV_SAMPLE_FMT_S32P、AV_SAMPLE_FMT_FLTP
    if (nb_channels == 2) { // 双声道（立体声）
        audio_encode_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // 采样格式
        av_channel_layout_from_mask(&audio_encode_ctx->ch_layout, AV_CH_LAYOUT_STEREO);
    } else { // 单声道
        audio_encode_ctx->sample_fmt = AV_SAMPLE_FMT_S16P; // 采样格式
        av_channel_layout_from_mask(&audio_encode_ctx->ch_layout, AV_CH_LAYOUT_MONO);
    }
    audio_encode_ctx->bit_rate = 64000; // 比特率，单位比特每秒
    audio_encode_ctx->sample_rate = 44100; // 采样率，单位次每秒
    int ret = avcodec_open2(audio_encode_ctx, audio_codec, NULL); // 打开编码器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
        return -1;
    }
    return 0;
}

// 把音频帧保存到MP3文件
int save_mp3_file(FILE *fp_out, AVFrame *frame) {
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(audio_encode_ctx, frame);
    while (ret == 0) {
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(audio_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 把编码后的MP3数据包写入文件
        fwrite(packet->data, 1, packet->size, fp_out);
    }
    av_packet_free(&packet); // 释放数据包资源
    return 0;
}

// 对音频帧解码
int decode_audio(AVPacket *packet, AVFrame *frame, FILE *fp_out, SwrContext *swr_ctx, AVFrame *swr_frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(audio_decode_ctx, packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(audio_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
        ret = swr_convert(swr_ctx, // 音频采样器的实例
                        // 输出的数据内容和数据大小
                        swr_frame->data, swr_frame->nb_samples,
                        // 输入的数据内容和数据大小
                        (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert frame occur error %d.\n", ret);
            return -1;
        }
        save_mp3_file(fp_out, swr_frame); // 把音频帧保存到MP3文件
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    if (argc > 1) {
        src_name = argv[1];
    }
    const char *mp3_name = "output_swrmp3.mp3";
    if (open_input_file(src_name) < 0) { // 打开输入文件
        return -1;
    }
    // 初始化MP3编码器的实例
    if (init_audio_encoder(audio_decode_ctx->ch_layout.nb_channels) < 0) {
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "audio_decode_ctx frame_size=%d, sample_fmt=%d, sample_rate=%d, nb_channels=%d\n", 
                                    audio_decode_ctx->frame_size,
                                    audio_decode_ctx->sample_fmt,
                                    audio_decode_ctx->sample_rate,
                                    audio_decode_ctx->ch_layout.nb_channels);
    av_log(NULL, AV_LOG_INFO, "audio_encode_ctx frame_size=%d, sample_fmt=%d, sample_rate=%d, nb_channels=%d\n", 
                                    audio_encode_ctx->frame_size,
                                    audio_encode_ctx->sample_fmt,
                                    audio_encode_ctx->sample_rate,
                                    audio_encode_ctx->ch_layout.nb_channels);

    FILE *fp_out = fopen(mp3_name, "wb"); // 以写方式打开输出文件
    if (!fp_out) {
        av_log(NULL, AV_LOG_ERROR, "open mp3 file %s fail.\n", mp3_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target audio file is %s\n", mp3_name);
    int ret = -1;
    // 开始初始化音频采样器的实例
    SwrContext *swr_ctx = NULL; // 音频采样器的实例
    ret = swr_alloc_set_opts2(&swr_ctx, // 音频采样器的实例
                          &audio_encode_ctx->ch_layout, // 输出的声道布局
                          audio_encode_ctx->sample_fmt, // 输出的采样格式
                          audio_encode_ctx->sample_rate, // 输出的采样频率
                          &audio_decode_ctx->ch_layout, // 输入的声道布局
                          audio_decode_ctx->sample_fmt, // 输入的采样格式
                          audio_decode_ctx->sample_rate, // 输入的采样频率
                          0, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "swr_alloc_set_opts2 error %d\n", ret);
        return -1;
    }
    ret = swr_init(swr_ctx); // 初始化音频采样器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "swr_init error %d\n", ret);
        return -1;
    }

    AVFrame *swr_frame = av_frame_alloc(); // 分配一个数据帧
    // 每帧的采样数量（帧大小）。这里要跟原来的音频保持一致
    swr_frame->nb_samples = audio_decode_ctx->frame_size;
    if (swr_frame->nb_samples <= 0) {
        swr_frame->nb_samples = 512;
    }
    swr_frame->format = audio_encode_ctx->sample_fmt; // 数据帧格式（采样格式）
    swr_frame->ch_layout = audio_encode_ctx->ch_layout; // 音频通道布局
    ret = av_frame_get_buffer(swr_frame, 0); // 为数据帧分配新的缓冲区
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "get frame buffer error %d\n", ret);
        return -1;
    }
    // 结束初始化音频采样器的实例
    
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要重新编码
            // 对音频帧解码
            decode_audio(packet, frame, fp_out, swr_ctx, swr_frame);
        }
        av_packet_unref(packet); // 清除数据包
    }
    while (1) { // 冲走重采样的缓存（兼容对ogg、amr等格式的重采样）
        // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
        ret = swr_convert(swr_ctx, // 音频采样器的实例
                        // 输出的数据内容和数据大小
                        swr_frame->data, swr_frame->nb_samples,
                        // 输入内容填NULL、输入大小填0表示冲走缓存
                        NULL, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert frame occur error %d.\n", ret);
            return -1;
        } else if (ret == 0) { // 到末尾了
            break;
        }
        save_mp3_file(fp_out, swr_frame); // 把音频帧保存到MP3文件
    }
    save_mp3_file(fp_out, NULL); // 传入一个空帧，冲走编码缓存
    av_log(NULL, AV_LOG_INFO, "Success resample audio frame as mp3 file.\n");
    fclose(fp_out); // 关闭输出文件
    
    av_frame_free(&swr_frame); // 释放数据帧资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    swr_free(&swr_ctx); // 释放音频采样器的实例
    avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avcodec_close(audio_encode_ctx); // 关闭音频编码器的实例
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}