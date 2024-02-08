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

AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
AVStream *dest_video = NULL; // 目标文件的视频流
AVCodecContext *video_encode_ctx = NULL; // 视频编码器的实例

// 打开输出文件
int open_output_file(const char *dest_name) {
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, dest_name);
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
    // 查找编码器
    AVCodec *video_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec) {
        av_log(NULL, AV_LOG_ERROR, "AV_CODEC_ID_H264 not found\n");
        return -1;
    }
    video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编解码器的实例
    if (!video_encode_ctx) {
        av_log(NULL, AV_LOG_ERROR, "video_encode_ctx is null\n");
        return -1;
    }
    video_encode_ctx->pix_fmt = AV_PIX_FMT_YUV420P; // 像素格式
    video_encode_ctx->width = 480; // 视频画面的宽度
    video_encode_ctx->height = 270; // 视频画面的高度
//    video_encode_ctx->width = 1440; // 视频画面的宽度
//    video_encode_ctx->height = 810; // 视频画面的高度。高度大等于578，播放器默认色度BT709；高度小等于576，播放器默认色度BT601
    video_encode_ctx->framerate = (AVRational){25, 1}; // 帧率
    video_encode_ctx->time_base = (AVRational){1, 25}; // 时间基
    // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
    if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    ret = avcodec_open2(video_encode_ctx, video_codec, NULL); // 打开编码器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open video_encode_ctx.\n");
        return -1;
    }
    // 创建指定编码器的数据流
    dest_video = avformat_new_stream(out_fmt_ctx, video_codec);
    // 把编码器实例中的参数复制给数据流
    avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
    dest_video->codecpar->codec_tag = 0; // 非特殊情况都填0
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");
    return 0;
}

// 给视频帧编码，并写入压缩后的视频包
int output_video(AVFrame *frame) {
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        AVPacket *packet = av_packet_alloc(); // 分配一个数据包
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(video_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dest_video->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *dest_name = "output_writeyuv.mp4";
    if (argc > 1) {
        dest_name = argv[1];
    }
    if (open_output_file(dest_name) < 0) { // 打开输出文件
        return -1;
    }
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    frame->format = video_encode_ctx->pix_fmt; // 像素格式
    frame->width  = video_encode_ctx->width; // 视频宽度
    frame->height = video_encode_ctx->height; // 视频高度
    int ret = av_frame_get_buffer(frame, 0); // 为数据帧分配缓冲区
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame data %d.\n", ret);
        return -1;
    }
    int index = 0;
    while (index < 200) { // 写入200帧
        ret = av_frame_make_writable(frame); // 确保数据帧是可写的
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't make frame writable %d.\n", ret);
            return -1;
        }
        int x, y;
        // 写入Y值
        for (y = 0; y < video_encode_ctx->height; y++)
            for (x = 0; x < video_encode_ctx->width; x++)
                frame->data[0][y * frame->linesize[0] + x] = 0; // Y值填0
        // 写入U值（Cb）和V值（Cr）
        for (y = 0; y < video_encode_ctx->height / 2; y++) {
            for (x = 0; x < video_encode_ctx->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 0; // U值填0
                frame->data[2][y * frame->linesize[2] + x] = 0; // V值填0
            }
        }
        frame->pts = index++; // 时间戳递增
        output_video(frame); // 给视频帧编码，并写入压缩后的视频包
    }
    av_log(NULL, AV_LOG_INFO, "Success write yuv video.\n");
    av_write_trailer(out_fmt_ctx); // 写文件尾
    
    av_frame_free(&frame); // 释放数据帧资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    return 0;
}
