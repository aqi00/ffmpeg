#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
};
#endif

AVCodecContext *audio_encode_ctx = NULL; // MP3编码器的实例

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
    av_packet_unref(packet); // 清除数据包
    av_packet_free(&packet); // 释放数据包资源
    return 0;
}

int main(int argc, char **argv) {
    const char *src_name = "../test.pcm";
    int nb_channels = 2; // 声道数量
    if (argc > 1) {
        src_name = argv[1];
    }
    if (argc > 2) {
        nb_channels = atoi(argv[2]);
    }
    const char *mp3_name = "output_pcm2mp3.mp3";
    FILE *fp_in = fopen(src_name, "rb"); // 以读方式打开输入文件
    if (!fp_in) {
        av_log(NULL, AV_LOG_ERROR, "open pcm file %s fail.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "source audio file is %s\n", src_name);
    FILE *fp_out = fopen(mp3_name, "wb"); // 以写方式打开输出文件
    if (!fp_out) {
        av_log(NULL, AV_LOG_ERROR, "open mp3 file %s fail.\n", mp3_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target audio file is %s\n", mp3_name);
    if (init_audio_encoder(nb_channels) < 0) { // 初始化MP3编码器的实例
        return -1;
    }
    
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    frame->nb_samples = audio_encode_ctx->frame_size; // 每帧的采样数量（帧大小）
    frame->format = audio_encode_ctx->sample_fmt; // 数据帧格式（采样格式）
    frame->ch_layout = audio_encode_ctx->ch_layout; // 音频通道布局
    int ret = av_frame_get_buffer(frame, 0); // 为数据帧分配新的缓冲区
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "get frame buffer error %d\n", ret);
        return -1;
    }
    int i=0, j=0, data_size=0;
    if (nb_channels == 2) { // 双声道（立体声）
        // 获取每个样本的大小，单位字节
        data_size = av_get_bytes_per_sample(audio_encode_ctx->sample_fmt);
        av_log(NULL, AV_LOG_INFO, "stereo data_size=%d\n", data_size);
        while (feof(fp_in) == 0) { // 轮询输入的pcm文件数据
            // 确保帧数据是可写的
            if ((ret = av_frame_make_writable(frame)) < 0) {
                av_log(NULL, AV_LOG_ERROR, "frame is not writable\n");
                return -1;
            }
            i = 0;
            while (i < frame->nb_samples) {
                j = 0;
                while (j < frame->ch_layout.nb_channels) {
                    fread(frame->data[j] + data_size * i, 1, data_size, fp_in);
                    j++;
                }
                i++;
            }
            save_mp3_file(fp_out, frame); // 把音频帧保存到MP3文件
        }
    } else { // 单声道
        // 获取指定音频参数所需的缓冲区大小
        data_size = av_samples_get_buffer_size(NULL, audio_encode_ctx->ch_layout.nb_channels, 
                        audio_encode_ctx->frame_size, audio_encode_ctx->sample_fmt, 1);
        av_log(NULL, AV_LOG_INFO, "mono data_size=%d\n", data_size);
        while (feof(fp_in) == 0) { // 轮询输入的pcm文件数据
            // 确保帧数据是可写的
            if ((ret = av_frame_make_writable(frame)) < 0) {
                av_log(NULL, AV_LOG_ERROR, "frame is not writable\n");
                return -1;
            }
            fread(frame->data[0], 1, data_size, fp_in);
            save_mp3_file(fp_out, frame); // 把音频帧保存到MP3文件
        }
    }
    save_mp3_file(fp_out, NULL); // 传入一个空帧，冲走编码缓存
    av_log(NULL, AV_LOG_INFO, "Success convert pcm file to mp3 file.\n");
    
    fclose(fp_out); // 关闭输出文件
    fclose(fp_in); // 关闭输入文件
    av_frame_free(&frame); // 释放数据帧资源
    avcodec_close(audio_encode_ctx); // 关闭音频编码器的实例
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    return 0;
}