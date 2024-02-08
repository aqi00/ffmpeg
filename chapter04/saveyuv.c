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

AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例

// 把视频帧保存为YUV图像。save_index表示要把第几个视频帧保存为图片
int save_yuv_file(AVFrame *frame, int save_index) {
    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    av_log(NULL, AV_LOG_INFO, "format = %d, width = %d, height = %d\n",
                            frame->format, frame->width, frame->height);
    char yuv_name[20] = { 0 };
    sprintf(yuv_name, "output_%03d.yuv", save_index);
    av_log(NULL, AV_LOG_INFO, "target image file is %s\n", yuv_name);
    FILE *fp = fopen(yuv_name, "wb"); // 以写方式打开文件
    if (!fp) {
        av_log(NULL, AV_LOG_ERROR, "open file %s fail.\n", yuv_name);
        return -1;
    }
    // 把YUV数据依次写入文件（按照YUV420P格式分解视频帧数据）
    int i = -1;
    while (++i < frame->height) { // 写入Y分量（灰度数值）
        fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, fp);
    }
    i = -1;
    while (++i < frame->height / 2) { // 写入U分量（色度数值的蓝色投影）
        fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, fp);
    }
    i = -1;
    while (++i < frame->height / 2) { // 写入V分量（色度数值的红色投影）
        fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, fp);
    }
    
    fclose(fp); // 关闭文件
    return 0;
}

int packet_index = -1; // 数据包的索引序号
// 对视频帧解码。save_index表示要把第几个视频帧保存为图片
int decode_video(AVPacket *packet, AVFrame *frame, int save_index) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        packet_index++;
        if (packet_index < save_index) { // 还没找到对应序号的帧
            return AVERROR(EAGAIN);
        }
        save_yuv_file(frame, save_index); // 把视频帧保存为YUV图像
        break;
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    int save_index = 0;
    if (argc > 1) {
        src_name = argv[1];
    }
    if (argc > 2) {
        save_index = atoi(argv[2]);
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
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        AVStream *src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "video_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }
    
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            ret = decode_video(packet, frame, save_index); // 对视频帧解码
            if (ret == 0) {
                break; // 只保存一幅图像就退出
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_log(NULL, AV_LOG_INFO, "Success save %d_index frame as yuv file.\n", save_index);
    
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}