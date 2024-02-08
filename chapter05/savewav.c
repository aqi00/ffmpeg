#include <stdio.h>
#include <sys/stat.h>

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

AVCodecContext *audio_decode_ctx = NULL; // 音频解码器的实例

typedef struct WAVHeader {
    char riffCkID[4]; // 固定填"RIFF"
    int32_t riffCkSize; // RIFF块大小。文件总长减去riffCkID和riffCkSize两个字段的长度
    char format[4]; // 固定填"WAVE"
    char fmtCkID[4]; // 固定填"fmt "
    int32_t fmtCkSize; // 格式块大小，从audioFormat到bitsPerSample各字段长度之和，为16
    int16_t audioFormat; // 音频格式。1表示整数，3表示浮点数
    int16_t channels; // 声道数量
    int32_t sampleRate; // 采样频率，单位赫兹
    int32_t byteRate; // 数据传输速率，单位字节每秒
    int16_t blockAlign; // 采样大小，即每个样本占用的字节数
    int16_t bitsPerSample; // 每个样本占用的比特数量，即采样大小乘以8（样本大小以字节为单位）
    char dataCkID[4]; // 固定填"data"
    int32_t dataCkSize; // 数据块大小。文件总长减去WAV头的长度
} WAVHeader;

// 把PCM文件转换为WAV文件
int save_wav_file(const char *pcm_name) {
    struct stat size; // 保存文件信息的结构
    if (stat(pcm_name, &size) != 0) { // 获取文件信息
        av_log(NULL, AV_LOG_ERROR, "file %s is not exists\n", pcm_name);
        return -1;
    }
    FILE *fp_pcm = fopen(pcm_name, "rb"); // 以读方式打开pcm文件
    if (!fp_pcm) {
        av_log(NULL, AV_LOG_ERROR, "open file %s fail.\n", pcm_name);
        return -1;
    }
    const char *wav_name = "output_savewav.wav";
    FILE *fp_wav = fopen(wav_name, "wb"); // 以写方式打开wav文件
    if (!fp_wav) {
        av_log(NULL, AV_LOG_ERROR, "open file %s fail.\n", wav_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target audio file is %s\n", wav_name);
    int pcmDataSize = size.st_size; // pcm文件大小
    av_log(NULL, AV_LOG_INFO, "pcmDataSize=%d\n", pcmDataSize);
    WAVHeader wavHeader; // wav文件头结构
    sprintf(wavHeader.riffCkID, "RIFF");
    // 设置 RIFF chunk size，RIFF chunk size 不包含 RIFF Chunk ID 和 RIFF Chunk Size的大小，所以用 PCM 数据大小加 RIFF 头信息大小减去 RIFF Chunk ID 和 RIFF Chunk Size的大小
    wavHeader.riffCkSize = (pcmDataSize + sizeof(WAVHeader) - 4 - 4);
    sprintf(wavHeader.format, "WAVE");
    sprintf(wavHeader.fmtCkID, "fmt ");
    wavHeader.fmtCkSize = 16;
    // 设置音频格式。1为整数，3为浮点数（含双精度数）
    if (audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP
        || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_FLT
        || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_DBLP
        || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_DBL) {
        wavHeader.audioFormat = 3;
    } else {
        wavHeader.audioFormat = 1;
    }
    wavHeader.channels = audio_decode_ctx->ch_layout.nb_channels; // 声道数量
    wavHeader.sampleRate = audio_decode_ctx->sample_rate; // 采样频率
    wavHeader.bitsPerSample = 8 * av_get_bytes_per_sample(audio_decode_ctx->sample_fmt);
    wavHeader.blockAlign = (wavHeader.channels * wavHeader.bitsPerSample) >> 3;
//    wavHeader.blockAlign = (wavHeader.channels * wavHeader.bitsPerSample) / 8;
    wavHeader.byteRate = wavHeader.sampleRate * wavHeader.blockAlign;
    sprintf(wavHeader.dataCkID, "data");
    // 设置数据块大小，即实际PCM数据的长度，单位字节
    wavHeader.dataCkSize = pcmDataSize;
    // 向wav文件写入wav文件头信息
    fwrite((const char *) &wavHeader, 1, sizeof(WAVHeader), fp_wav);
    const int per_size = 1024; // 每次读取的大小
    uint8_t *per_buff = (uint8_t*)av_malloc(per_size); // 读取缓冲区
    int len = 0;
    // 循环读取PCM文件中的音频数据
    while ((len = fread(per_buff, 1, per_size, fp_pcm)) > 0) {
        fwrite(per_buff, 1, per_size, fp_wav); // 依次写入每个PCM数据
    }
    fclose(fp_pcm); // 关闭pcm文件
    fclose(fp_wav); // 关闭wav文件
    return 0;
}

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    if (argc > 1) {
        src_name = argv[1];
    }
    const char *pcm_name = "output_savewav.pcm";
    
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
                // 把音频帧保存为PCM音频
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
    save_wav_file(pcm_name); // 把PCM文件转换为WAV文件
    av_log(NULL, AV_LOG_INFO, "Success save audio frame as wav file.\n");
    
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}