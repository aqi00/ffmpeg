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

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    if (argc > 1) {
        src_name = argv[1];
    }
    const char *pcm_name = "output_savepcm.pcm";
    
    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
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
    AVCodecContext *audio_decode_ctx = NULL; // 音频解码器的实例
    // 找到音频流的索引
    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
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
        // 音频帧的format字段为AVSampleFormat枚举类型，为8时表示AV_SAMPLE_FMT_FLTP
        av_log(NULL, AV_LOG_INFO, "sample_fmt=%d, nb_samples=%d, nb_channels=%d, ",
                    audio_decode_ctx->sample_fmt, audio_decode_ctx->frame_size, 
                    audio_decode_ctx->ch_layout.nb_channels);
        av_log(NULL, AV_LOG_INFO, "format_name=%s, is_planar=%d, data_size=%d\n",
                    av_get_sample_fmt_name(audio_decode_ctx->sample_fmt),
                    av_sample_fmt_is_planar(audio_decode_ctx->sample_fmt), 
                    av_get_bytes_per_sample(audio_decode_ctx->sample_fmt));
        ret = avcodec_open2(audio_decode_ctx, audio_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find audio stream.\n");
        return -1;
    }
    FILE *fp_out = fopen(pcm_name, "wb"); // 以写方式打开文件
    if (!fp_out) {
        av_log(NULL, AV_LOG_ERROR, "open file %s fail.\n", pcm_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target audio file is %s\n", pcm_name);
    
    int i=0, j=0, data_size=0;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要重新编码
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0) {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(audio_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                } else if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                    continue;
                }
                // 把音频帧保存到PCM文件
                if (av_sample_fmt_is_planar((enum AVSampleFormat)frame->format)) {
                    // 平面模式的音频在存储时要改为交错模式
                    data_size = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
                    i = 0;
                    while (i < frame->nb_samples) {
                        j = 0;
                        while (j < frame->ch_layout.nb_channels) {
                            fwrite(frame->data[j] + data_size*i, 1, data_size, fp_out);
                            j++;
                        }
                        i++;
                    }
                } else { // 非平面模式，直接写入文件
                    fwrite(frame->extended_data[0], 1, frame->linesize[0], fp_out);
                }
                av_frame_unref(frame); // 清除数据帧
            } else {
                av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    fclose(fp_out); // 关闭文件
    av_log(NULL, AV_LOG_INFO, "Success save audio frame as pcm file.\n");
    
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}