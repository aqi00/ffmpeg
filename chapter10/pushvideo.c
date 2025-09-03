#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#ifdef __cplusplus
};
#endif

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    const char *dest_name = "rtsp://127.0.0.1:8554/stream";
    if (argc > 1) {
        src_name = argv[1];
    }
    if (argc > 2) {
        dest_name = argv[2];
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
    AVStream *src_video = NULL;
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
    }
    
    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    // 分配音视频文件的封装实例（注意rtmp协议的第三个参数填flv，rtsp协议的第三个参数填rtsp）
    if (strstr(dest_name, "rtmp") != NULL) {
        ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, "flv", dest_name);
    } else {
        ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, "rtsp", dest_name);
    }
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't alloc output_file %s.\n", dest_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open push url %s.\n", dest_name);
    // 打开输出流（注意rtsp推流不要调用avio_open，但rtmp推流要调用avio_open）
    if (strstr(dest_name, "rtmp") != NULL) {
        ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open output_file %s.\n", dest_name);
            return -1;
        }
    }
    AVStream *dest_video = NULL;
    if (video_index >= 0) { // 源文件有视频流，就给目标文件创建视频流
        dest_video = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的视频参数原样复制过来
        avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
        dest_video->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");
    av_log(NULL, AV_LOG_INFO, "src_video->time_base %lld/%lld, dest_video->time_base %lld/%lld\n", 
                        src_video->time_base.num, src_video->time_base.den, 
                        dest_video->time_base.num, dest_video->time_base.den);
    
    int64_t start_time = av_gettime(); // 获取当前时间，单位微秒
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) {
            av_log(NULL, AV_LOG_INFO, "%lld ", packet->pts);
            // 把数据包的时间戳从一个时间基转换为另一个时间基
            av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
            int64_t pass_time = av_gettime() - start_time; // 计算已经流逝的时间
            int64_t dts_time = packet->dts * (1000 * 1000 * av_q2d(dest_video->time_base));
            if (dts_time > pass_time) { // 尚未到达播放时间
                av_usleep(dts_time - pass_time); // 睡眠若干时间，单位微秒
            }
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_log(NULL, AV_LOG_INFO, "\n");
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success push video stream.\n");
    
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}