#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
};
#endif

AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例

// 把视频帧保存为JPEG图片。save_index表示要把第几个视频帧保存为图片
int save_jpg_file(AVFrame *frame, int save_index) {
    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    av_log(NULL, AV_LOG_INFO, "format = %d, width = %d, height = %d\n",
                            frame->format, frame->width, frame->height);
    char jpg_name[20] = { 0 };
    sprintf(jpg_name, "output_%03d.jpg", save_index);
    av_log(NULL, AV_LOG_INFO, "target image file is %s\n", jpg_name);
    
    enum AVPixelFormat target_format = AV_PIX_FMT_YUVJ420P; // jpg的像素格式是YUVJ420P
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    struct SwsContext *swsContext = sws_getContext(
            frame->width, frame->height, AV_PIX_FMT_YUV420P, 
            frame->width, frame->height, target_format, 
            SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (swsContext == NULL) {
        av_log(NULL, AV_LOG_ERROR, "swsContext is null\n");
        return -1;
    }
    AVFrame *yuvj_frame = av_frame_alloc(); // 分配一个YUVJ数据帧
    yuvj_frame->format = target_format; // 像素格式
    yuvj_frame->width = frame->width; // 视频宽度
    yuvj_frame->height = frame->height; // 视频高度
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(yuvj_frame->data, yuvj_frame->linesize, 
            frame->width, frame->height, target_format, 1);
//    // 分配缓冲区空间，用于存放转换后的图像数据
//    int buffer_size = av_image_get_buffer_size(target_format, frame->width, frame->height, 1);
//    unsigned char *out_buffer = (unsigned char*)av_malloc(
//                            (size_t)buffer_size * sizeof(unsigned char));
//    // 将数据帧与缓冲区关联
//    av_image_fill_arrays(yuvj_frame->data, yuvj_frame->linesize, out_buffer,
//                       target_format, frame->width, frame->height, 1);
    // 转换器开始处理图像数据，把YUV图像转为YUVJ图像
    sws_scale(swsContext, (const uint8_t* const*) frame->data, frame->linesize,
        0, frame->height, yuvj_frame->data, yuvj_frame->linesize);
    sws_freeContext(swsContext); // 释放图像转换器的实例

    AVFormatContext *jpg_fmt_ctx = NULL;
    // 分配JPEG文件的封装实例
    int ret = avformat_alloc_output_context2(&jpg_fmt_ctx, NULL, NULL, jpg_name);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't alloc output_file %s.\n", jpg_name);
        return -1;
    }
    // 查找MJPEG编码器
    AVCodec *jpg_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpg_codec) {
        av_log(NULL, AV_LOG_ERROR, "jpg_codec not found\n");
        return -1;
    }
    // 获取编解码器上下文信息
    AVCodecContext *jpg_encode_ctx = avcodec_alloc_context3(jpg_codec);
    if (!jpg_encode_ctx) {
        av_log(NULL, AV_LOG_ERROR, "jpg_encode_ctx is null\n");
        return -1;
    }
    // jpg的像素格式是YUVJ，对于MJPEG编码器来说，它支持YUVJ420P/YUVJ422P/YUVJ444P等格式
    jpg_encode_ctx->pix_fmt = target_format; // 像素格式
    jpg_encode_ctx->width = frame->width; // 视频宽度
    jpg_encode_ctx->height = frame->height; // 视频高度
    jpg_encode_ctx->time_base = (AVRational){1, 25}; // 时间基
    av_log(NULL, AV_LOG_INFO, "jpg codec_id = %d\n",jpg_encode_ctx->codec_id);
    ret = avcodec_open2(jpg_encode_ctx, jpg_codec, NULL); // 打开编码器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open jpg_encode_ctx.\n");
        return -1;
    }
    AVStream *jpg_stream = avformat_new_stream(jpg_fmt_ctx, 0); // 创建数据流
    ret = avformat_write_header(jpg_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    // 把YUVJ数据帧发给编码器实例
    ret = avcodec_send_frame(jpg_encode_ctx, yuvj_frame);
    while (ret == 0) {
        packet->stream_index = 0;
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(jpg_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        ret = av_write_frame(jpg_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
    }
    av_write_trailer(jpg_fmt_ctx); // 写入文件尾
    av_frame_free(&yuvj_frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(jpg_fmt_ctx->pb); // 关闭输出流
    avcodec_close(jpg_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&jpg_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(jpg_fmt_ctx); // 释放封装器的实例
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
        save_jpg_file(frame, save_index); // 把视频帧保存为JPEG图片
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
    av_log(NULL, AV_LOG_INFO, "Success save %d_index frame as jpg file.\n", save_index);
    
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}