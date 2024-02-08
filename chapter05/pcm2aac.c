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

AVCodecContext *audio_encode_ctx = NULL; // AAC编码器的实例

// 获取ADTS头部
void get_adts_header(AVCodecContext *codec_ctx, char *adts_header, int aac_length) {
    uint8_t freq_index = 0; // 采样频率对应的索引
    switch (codec_ctx->sample_rate) {
        case 96000: freq_index = 0; break;
        case 88200: freq_index = 1; break;
        case 64000: freq_index = 2; break;
        case 48000: freq_index = 3; break;
        case 44100: freq_index = 4; break;
        case 32000: freq_index = 5; break;
        case 24000: freq_index = 6; break;
        case 22050: freq_index = 7; break;
        case 16000: freq_index = 8; break;
        case 12000: freq_index = 9; break;
        case 11025: freq_index = 10; break;
        case 8000: freq_index = 11; break;
        case 7350: freq_index = 12; break;
        default: freq_index = 4; break;
    }
    uint8_t nb_channels = codec_ctx->ch_layout.nb_channels; // 声道数量
    uint32_t frame_length = aac_length + 7; // adts头部的长度为7个字节
    adts_header[0] = 0xFF; // 二进制值固定为 1111 1111
    // 二进制值为 1111 0001。其中前四位固定填1；第五位填0表示MPEG-4，填1表示MPEG-2；六七两位固定填0；第八位填0表示adts头长度9字节，填1表示adts头长度7字节
    adts_header[1] = 0xF1;
    // 二进制前两位表示AAC音频规格；中间四位表示采样率的索引；第七位填0即可；第八位填声道数量除以四的商
    adts_header[2] = ((codec_ctx->profile) << 6) + (freq_index << 2) + (nb_channels >> 2);
    // 二进制前两位填声道数量除以四的余数；中间四位填0即可；后面两位填frame_length的前2位（frame_length总长13位）
    adts_header[3] = (((nb_channels & 3) << 6) + (frame_length  >> 11));
    // 二进制填frame_length的第3位到第10位，“& 0x7FF”表示先取后面11位（掐掉前两位），“>> 3”表示再截掉末尾的3位，结果就取到了中间的8位
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    // 二进制前3位填frame_length的后3位，“& 7”表示取后3位（7的二进制是111）；后5位填1
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    // 二进制前6位填1；后2位填0，表示一个ADTS帧包含一个AAC帧，就是每帧ADTS包含的AAC帧数量减1
    adts_header[6] = 0xFC;
    return;
}

// 初始化AAC编码器的实例
int init_audio_encoder(int nb_channels) {
    // 查找AAC编码器
    AVCodec *audio_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_AAC);
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
    if (nb_channels == 2) { // 双声道（立体声）
        av_channel_layout_from_mask(&audio_encode_ctx->ch_layout, AV_CH_LAYOUT_STEREO);
    } else { // 单声道
        av_channel_layout_from_mask(&audio_encode_ctx->ch_layout, AV_CH_LAYOUT_MONO);
    }
    // FFmpeg自带的aac只支持AV_SAMPLE_FMT_FLTP编码
    audio_encode_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // 采样格式
    audio_encode_ctx->bit_rate = 128000; // 比特率，单位比特每秒
    audio_encode_ctx->sample_rate = 44100; // 采样率，单位次每秒
    audio_encode_ctx->profile = FF_PROFILE_AAC_LOW; // AAC规格
    int ret = avcodec_open2(audio_encode_ctx, audio_codec, NULL); // 打开编码器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "audio_encode_ctx->profile=%d\n", audio_encode_ctx->profile);
    return 0;
}

// 把音频帧保存到AAC文件
int save_aac_file(FILE *fp_out, AVFrame *frame) {
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
        char head[7] = {0};
        // AAC格式需要获取ADTS头部
        get_adts_header(audio_encode_ctx, head, packet->size);
        fwrite(head, 1, sizeof(head), fp_out); // 写入ADTS头部
        // 把编码后的AAC数据包写入文件
        int len = fwrite(packet->data, 1, packet->size, fp_out);
        if (len != packet->size) {
            av_log(NULL, AV_LOG_ERROR, "fwrite aac error\n");
            return -1;
        }
    }
    av_packet_unref(packet); // 清除数据包
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
    const char *aac_name = "output_pcm2aac.aac";
    FILE *fp_in = fopen(src_name, "rb"); // 以读方式打开输入文件
    if (!fp_in) {
        av_log(NULL, AV_LOG_ERROR, "open pcm file %s fail.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "source audio file is %s\n", src_name);
    FILE *fp_out = fopen(aac_name, "wb"); // 以写方式打开输出文件
    if (!fp_out) {
        av_log(NULL, AV_LOG_ERROR, "open aac file %s fail.\n", aac_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target audio file is %s\n", aac_name);
    if (init_audio_encoder(nb_channels) < 0) { // 初始化AAC编码器的实例
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
            save_aac_file(fp_out, frame); // 把音频帧保存到AAC文件
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
            save_aac_file(fp_out, frame); // 把音频帧保存到AAC文件
        }
    }
    save_aac_file(fp_out, NULL); // 传入一个空帧，冲走编码缓存
    av_log(NULL, AV_LOG_INFO, "Success convert pcm file to aac file.\n");
    
    fclose(fp_out); // 关闭输出文件
    fclose(fp_in); // 关闭输入文件
    av_frame_free(&frame); // 释放数据帧资源
    avcodec_close(audio_encode_ctx); // 关闭音频编码器的实例
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    return 0;
}