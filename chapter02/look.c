#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
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
    // 格式化输出文件信息
    av_dump_format(fmt_ctx, 0, filename, 0);
    av_log(NULL, AV_LOG_INFO, "duration=%d\n", fmt_ctx->duration); // 持续时间，单位微秒
    av_log(NULL, AV_LOG_INFO, "bit_rate=%d\n", fmt_ctx->bit_rate); // 比特率，单位比特每秒
    av_log(NULL, AV_LOG_INFO, "nb_streams=%d\n", fmt_ctx->nb_streams); // 数据流的数量
    av_log(NULL, AV_LOG_INFO, "max_streams=%d\n", fmt_ctx->max_streams); // 数据流的最大数量
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    av_log(NULL, AV_LOG_INFO, "video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        av_log(NULL, AV_LOG_INFO, "video_stream index=%d\n", video_stream->index);
        av_log(NULL, AV_LOG_INFO, "video_stream start_time=%d\n", video_stream->start_time);
        av_log(NULL, AV_LOG_INFO, "video_stream nb_frames=%d\n", video_stream->nb_frames);
        av_log(NULL, AV_LOG_INFO, "video_stream duration=%d\n", video_stream->duration);
    }
    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    av_log(NULL, AV_LOG_INFO, "audio_index=%d\n", audio_index);
    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        av_log(NULL, AV_LOG_INFO, "audio_stream index=%d\n", audio_stream->index);
        av_log(NULL, AV_LOG_INFO, "audio_stream start_time=%d\n", audio_stream->start_time);
        av_log(NULL, AV_LOG_INFO, "audio_stream nb_frames=%d\n", audio_stream->nb_frames);
        av_log(NULL, AV_LOG_INFO, "audio_stream duration=%d\n", audio_stream->duration);
    }
    avformat_close_input(&fmt_ctx); // 关闭音视频文件
    return 0;
}