#include <stdio.h>
#include <SDL.h>
// 引入SDL要增加下面的声明#undef main，否则编译会报错“undefined reference to `WinMain'”
#undef main

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

#define MAX_AUDIO_FRAME_SIZE 8096 // 一帧音频最大长度（样本数），该值不能太小
int audio_len = 0; // 一帧PCM音频的数据长度
unsigned char *audio_pos = NULL; // 当前读取的位置

// 回调函数，在获取音频数据后调用
void fill_audio(void *para, uint8_t *stream, int len) {
    SDL_memset(stream, 0, len); // 将缓冲区清零
    if (audio_len == 0) {
        return;
    }
    len = (len > audio_len ? audio_len : len);
    // 将音频数据混合到缓冲区
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}

int main(int argc, char **argv) {
    const char *src_name = "../plum.mp3";
    if (argc > 1) {
        src_name = argv[1];
    }
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
    AVStream *src_audio = NULL;
    // 找到音频流的索引
    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0) {
        src_audio = in_fmt_ctx->streams[audio_index];
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
    
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的声道布局
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; // 输出的采样格式
    int out_sample_rate = 44100; // 输出的采样率
    int out_nb_samples = audio_decode_ctx->frame_size; // 输出的采样数量
    int out_channels = out_ch_layout.nb_channels; // 输出的声道数量
    if (out_nb_samples == 0) {
        av_log(NULL, AV_LOG_ERROR, "unknown audio nb_samples\n");
        return -1;
    }
    SwrContext *swr_ctx = NULL; // 音频采样器的实例
    ret = swr_alloc_set_opts2(&swr_ctx, // 音频采样器的实例
                          &out_ch_layout, // 输出的声道布局
                          out_sample_fmt, // 输出的采样格式
                          out_sample_rate, // 输出的采样频率
                          &audio_decode_ctx->ch_layout, // 输入的声道布局
                          audio_decode_ctx->sample_fmt, // 输入的采样格式
                          audio_decode_ctx->sample_rate, // 输入的采样频率
                          0, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "swr_alloc_set_opts2 error %d\n", ret);
        return -1;
    }
    swr_init(swr_ctx); // 初始化音频采样器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "swr_init error %d\n", ret);
        return -1;
    }
    // 计算输出的缓冲区大小
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
    // 分配输出缓冲区的空间
    unsigned char *out_buff = (unsigned char *) av_malloc(MAX_AUDIO_FRAME_SIZE * out_channels);

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        av_log(NULL, AV_LOG_ERROR, "can not initialize SDL\n");
        return -1;
    }
    SDL_AudioSpec audio_spec; // 声明SDL音频参数
    audio_spec.freq = out_sample_rate; // 采样频率
    audio_spec.format = AUDIO_S16SYS; // 采样格式
    audio_spec.channels = out_channels; // 声道数量
    audio_spec.silence = 0; // 是否静音
    audio_spec.samples = out_nb_samples; // 采样数量
    audio_spec.callback = fill_audio; // 回调函数的名称
    audio_spec.userdata = NULL; // 回调函数的额外信息，如果没有额外信息就填NULL
    if (SDL_OpenAudio(&audio_spec, NULL) < 0) { // 打开扬声器
        av_log(NULL, AV_LOG_ERROR, "open audio occur error\n");
        return -1;
    }
    SDL_PauseAudio(0); // 播放/暂停音频。参数为0表示播放，为1表示暂停

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要解码
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
                //av_log(NULL, AV_LOG_INFO, "%d ", frame->nb_samples);
                av_log(NULL, AV_LOG_INFO, "%d ", packet->pts);
                while (audio_len > 0) { // 如果还没播放完，就等待1ms
                    SDL_Delay(1); // 延迟若干时间，单位毫秒
                }
                // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
                swr_convert(swr_ctx, // 音频采样器的实例
                    &out_buff, MAX_AUDIO_FRAME_SIZE, // 输出的数据内容和数据大小
                    (const uint8_t **) frame->data, frame->nb_samples); // 输入的数据内容和数据大小
                audio_pos = (unsigned char *) out_buff; // 把音频数据同步到缓冲区位置
                audio_len = out_buffer_size; // 缓冲区大小
            } else {
                av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_log(NULL, AV_LOG_INFO, "Success play audio file.\n");
    
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(audio_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    swr_free(&swr_ctx); // 释放音频采样器的实例
    
    SDL_CloseAudio(); // 关闭扬声器
    SDL_Quit(); // 退出SDL
    av_log(NULL, AV_LOG_INFO, "Quit SDL.\n");
    return 0;
}
