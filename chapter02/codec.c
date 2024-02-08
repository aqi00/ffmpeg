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
    av_log(NULL, AV_LOG_INFO, "Success find stream information.\n");
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
//        av_log(NULL, AV_LOG_INFO, "video_stream codec_id=%d\n", video_codec_id);
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        av_log(NULL, AV_LOG_INFO, "video_codec id=%d\n", video_codec->id);
        av_log(NULL, AV_LOG_INFO, "video_codec name=%s\n", video_codec->name);
        av_log(NULL, AV_LOG_INFO, "video_codec long_name=%s\n", video_codec->long_name);
        // 下面的type字段来自AVMediaType定义，为0表示AVMEDIA_TYPE_VIDEO，为1表示AVMEDIA_TYPE_AUDIO
        av_log(NULL, AV_LOG_INFO, "video_codec type=%d\n", video_codec->type);
    }
    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    av_log(NULL, AV_LOG_INFO, "audio_index=%d\n", audio_index);
    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
//        av_log(NULL, AV_LOG_INFO, "audio_stream codec_id=%d\n", audio_codec_id);
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec*) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        av_log(NULL, AV_LOG_INFO, "audio_codec id=%d\n", audio_codec->id);
        av_log(NULL, AV_LOG_INFO, "audio_codec name=%s\n", audio_codec->name);
        av_log(NULL, AV_LOG_INFO, "audio_codec long_name=%s\n", audio_codec->long_name);
        av_log(NULL, AV_LOG_INFO, "audio_codec type=%d\n", audio_codec->type);
    }
    avformat_close_input(&fmt_ctx); // 关闭音视频文件
    return 0;
}