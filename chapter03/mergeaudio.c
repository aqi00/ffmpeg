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
    const char *video_name = "../fuzhous.mp4";
    const char *audio_name = "../fuzhous.aac";
    const char *dest_name = "output_mergeaudio.mp4";
    if (argc > 1) {
        video_name = argv[1];
    }
    if (argc > 2) {
        audio_name = argv[2];
    }
    if (argc > 3) {
        dest_name = argv[3];
    }
    AVFormatContext *video_fmt_ctx = NULL; // 输入文件的封装器实例
    // 打开视频文件
    int ret = avformat_open_input(&video_fmt_ctx, video_name, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", video_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", video_name);
    // 查找视频文件中的流信息
    ret = avformat_find_stream_info(video_fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    AVStream *src_video = NULL;
    // 找到视频流的索引
    int video_index = av_find_best_stream(video_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        src_video = video_fmt_ctx->streams[video_index];
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }
    AVFormatContext *audio_fmt_ctx = NULL; // 输入文件的封装器实例
    // 打开音频文件
    ret = avformat_open_input(&audio_fmt_ctx, audio_name, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", audio_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", audio_name);
    // 查找音频文件中的流信息
    ret = avformat_find_stream_info(audio_fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    AVStream *src_audio = NULL;
    // 找到音频流的索引
    int audio_index = av_find_best_stream(audio_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0) {
        src_audio = audio_fmt_ctx->streams[audio_index];
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find audio stream.\n");
        return -1;
    }
    
    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    // 分配音视频文件的封装实例
    ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, dest_name);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't alloc output_file %s.\n", dest_name);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open output_file %s.\n", dest_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open output_file %s.\n", dest_name);
    AVStream *dest_video = NULL;
    if (video_index >= 0) { // 源文件有视频流，就给目标文件创建视频流
        dest_video = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的视频参数原样复制过来
        avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
        // 如果后面有对视频帧转换时间基，这里就无需复制时间基
        //dest_video->time_base = src_video->time_base;
        dest_video->codecpar->codec_tag = 0;
    }
    AVStream *dest_audio = NULL;
    if (audio_index >= 0) { // 源文件有音频流，就给目标文件创建音频流
        dest_audio = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
        dest_audio->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");
    
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    int64_t last_video_pts = 0; // 上次的视频时间戳
    int64_t last_audio_pts = 0; // 上次的音频时间戳
    while (1) {
        // av_compare_ts函数用于比较两个时间戳的大小（它们来自不同的时间基）
        if (last_video_pts==0 || av_compare_ts(last_video_pts, dest_video->time_base,
                                    last_audio_pts, dest_audio->time_base) <= 0) {
            while ((ret = av_read_frame(video_fmt_ctx, packet)) >= 0) { // 轮询视频包
                if (packet->stream_index == video_index) { // 找到一个视频包
                    break;
                }
            }
            if (ret == 0) {
                // av_packet_rescale_ts会把数据包的时间戳从一个时间基转换为另一个时间基
                av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
                packet->stream_index = 0; // 视频流索引
                last_video_pts = packet->pts; // 保存最后一次视频时间戳
            } else {
                av_log(NULL, AV_LOG_INFO, "End video file.\n");
                break;
            }
        } else {
            while ((ret = av_read_frame(audio_fmt_ctx, packet)) >= 0) { // 轮询音频包
                if (packet->stream_index == audio_index) { // 找到一个音频包
                    break;
                }
            }
            if (ret == 0) {
                // av_packet_rescale_ts会把数据包的时间戳从一个时间基转换为另一个时间基
                av_packet_rescale_ts(packet, src_audio->time_base, dest_audio->time_base);
                packet->stream_index = 1; // 音频流索引
                last_audio_pts = packet->pts; // 保存最后一次音频时间戳
            } else {
                av_log(NULL, AV_LOG_INFO, "End audio file.\n");
                break;
            }
        }
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success merge video and audio file.\n");
    
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&video_fmt_ctx); // 关闭视频文件
    avformat_close_input(&audio_fmt_ctx); // 关闭音频文件
    return 0;
}