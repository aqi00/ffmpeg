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

//    AVRational r_frame_rate;
//    
//typedef struct AVRational{
//    int num; ///< Numerator，分子
//    int den; ///< Denominator，分母
//} AVRational; // Rational，定量

int main(int argc, char **argv) {
    const char *filename = "../fuzhou.mp4";
    if (argc > 1) {
        filename = argv[1];
    }
    AVFormatContext *fmt_ctx = NULL;
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", filename);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", filename);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "duration=%d\n", fmt_ctx->duration); // 持续时间，单位微秒
    av_log(NULL, AV_LOG_INFO, "nb_streams=%d\n", fmt_ctx->nb_streams); // 数据流的数量
    av_log(NULL, AV_LOG_INFO, "max_streams=%d\n", fmt_ctx->max_streams); // 数据流的最大数量
    av_log(NULL, AV_LOG_INFO, "video_codec_id=%d\n", fmt_ctx->video_codec_id);
    av_log(NULL, AV_LOG_INFO, "audio_codec_id=%d\n", fmt_ctx->audio_codec_id);
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    av_log(NULL, AV_LOG_INFO, "video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        av_log(NULL, AV_LOG_INFO, "video_codec name=%s\n", video_codec->name);
        AVCodecParameters *video_codecpar = video_stream->codecpar;
        // 计算帧率，每秒有几个视频帧
        int fps = video_stream->r_frame_rate.num/video_stream->r_frame_rate.den;
        //int fps = av_q2d(video_stream->r_frame_rate);
        av_log(NULL, AV_LOG_INFO, "video_codecpar bit_rate=%d\n", video_codecpar->bit_rate);
        av_log(NULL, AV_LOG_INFO, "video_codecpar width=%d\n", video_codecpar->width);
        av_log(NULL, AV_LOG_INFO, "video_codecpar height=%d\n", video_codecpar->height);
        av_log(NULL, AV_LOG_INFO, "video_stream fps=%d\n", fps);
        int per_video = 1000 / fps; // 计算每个视频帧的持续时间
        av_log(NULL, AV_LOG_INFO, "one video frame's duration is %dms\n", per_video);
        // 获取视频的时间基准
        AVRational time_base = video_stream->time_base;
        av_log(NULL, AV_LOG_INFO, "video_stream time_base.num=%d\n", time_base.num);
        av_log(NULL, AV_LOG_INFO, "video_stream time_base.den=%d\n", time_base.den);
        // 计算视频帧的时间戳增量
        int timestamp_increment = 1 * time_base.den / fps;
        av_log(NULL, AV_LOG_INFO, "video timestamp_increment=%d\n", timestamp_increment);
    }
    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    av_log(NULL, AV_LOG_INFO, "audio_index=%d\n", audio_index);
    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec*) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        av_log(NULL, AV_LOG_INFO, "audio_codec name=%s\n", audio_codec->name);
        AVCodecParameters *audio_codecpar = audio_stream->codecpar;
        av_log(NULL, AV_LOG_INFO, "audio_codecpar bit_rate=%d\n", audio_codecpar->bit_rate);
        av_log(NULL, AV_LOG_INFO, "audio_codecpar frame_size=%d\n", audio_codecpar->frame_size);
        av_log(NULL, AV_LOG_INFO, "audio_codecpar sample_rate=%d\n", audio_codecpar->sample_rate);
        av_log(NULL, AV_LOG_INFO, "audio_codecpar nb_channels=%d\n", audio_codecpar->ch_layout.nb_channels);
        // 计算音频帧的持续时间。frame_size为每个音频帧的采样数量，sample_rate为音频帧的采样频率
        int per_audio = 1000 * audio_codecpar->frame_size / audio_codecpar->sample_rate;
        av_log(NULL, AV_LOG_INFO, "one audio frame's duration is %dms\n", per_audio);
        // 获取音频的时间基准
        AVRational time_base = audio_stream->time_base;
        av_log(NULL, AV_LOG_INFO, "audio_stream time_base.num=%d\n", time_base.num);
        av_log(NULL, AV_LOG_INFO, "audio_stream time_base.den=%d\n", time_base.den);
        // 计算音频帧的时间戳增量
        int timestamp_increment = 1 * audio_codecpar->frame_size * (time_base.den / audio_codecpar->sample_rate);
        av_log(NULL, AV_LOG_INFO, "audio timestamp_increment=%d\n", timestamp_increment);
    }
    avformat_close_input(&fmt_ctx); // 关闭音视频文件
    return 0;
}