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

AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例
int video_index = -1; // 视频流的索引
AVFormatContext *gif_fmt_ctx = NULL; // GIF图片的封装器实例
AVCodecContext *gif_encode_ctx = NULL; // GIF编码器的实例
AVStream *src_video = NULL; // 源文件的视频流
AVStream *gif_stream = NULL;  // GIF数据流
enum AVPixelFormat target_format = AV_PIX_FMT_BGR8; // gif的像素格式
struct SwsContext *swsContext = NULL; // 图像转换器的实例
AVFrame *rgb_frame = NULL; // RGB数据帧

// 打开输入文件
int open_input_file(const char *src_name) {
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
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
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
    return 0;
}

// 打开输出文件
int open_output_file(const char *gif_name) {
    // 分配GIF文件的封装实例
    int ret = avformat_alloc_output_context2(&gif_fmt_ctx, NULL, NULL, gif_name);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't alloc output_file %s.\n", gif_name);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&gif_fmt_ctx->pb, gif_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open output_file %s.\n", gif_name);
        return -1;
    }
    // 查找GIF编码器
    AVCodec *gif_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_GIF);
    if (!gif_codec) {
        av_log(NULL, AV_LOG_ERROR, "gif_codec not found\n");
        return -1;
    }
    // 获取编解码器上下文信息
    gif_encode_ctx = avcodec_alloc_context3(gif_codec);
    if (!gif_encode_ctx) {
        av_log(NULL, AV_LOG_ERROR, "gif_encode_ctx is null\n");
        return -1;
    }
    gif_encode_ctx->pix_fmt = target_format; // 像素格式
    gif_encode_ctx->width = video_decode_ctx->width; // 视频宽度
    gif_encode_ctx->height = video_decode_ctx->height; // 视频高度
    gif_encode_ctx->time_base = src_video->time_base; // 时间基
    av_log(NULL, AV_LOG_INFO, "gif codec_id = %d\n", gif_encode_ctx->codec_id);
    ret = avcodec_open2(gif_encode_ctx, gif_codec, NULL); // 打开编码器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open gif_encode_ctx.\n");
        return -1;
    }
    gif_stream = avformat_new_stream(gif_fmt_ctx, 0); // 创建数据流
    // 把编码器实例的参数复制给目标视频流
    avcodec_parameters_from_context(gif_stream->codecpar, gif_encode_ctx);
    gif_stream->codecpar->codec_tag = 0;
    ret = avformat_write_header(gif_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    return 0;
}

// 初始化图像转换器的实例
int init_sws_context(void) {
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    swsContext = sws_getContext(
            video_decode_ctx->width, video_decode_ctx->height, AV_PIX_FMT_YUV420P, 
            video_decode_ctx->width, video_decode_ctx->height, target_format, 
            SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (swsContext == NULL) {
        av_log(NULL, AV_LOG_ERROR, "swsContext is null\n");
        return -1;
    }
    rgb_frame = av_frame_alloc(); // 分配一个RGB数据帧
    rgb_frame->format = target_format; // 像素格式
    rgb_frame->width = video_decode_ctx->width; // 视频宽度
    rgb_frame->height = video_decode_ctx->height; // 视频高度
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(rgb_frame->data, rgb_frame->linesize, 
            video_decode_ctx->width, video_decode_ctx->height, target_format, 1);
//    // 分配缓冲区空间，用于存放转换后的图像数据
//    int buffer_size = av_image_get_buffer_size(target_format, video_decode_ctx->width, video_decode_ctx->height, 1);
//    unsigned char *out_buffer = (unsigned char*)av_malloc(
//                            (size_t)buffer_size * sizeof(unsigned char));
//    // 将数据帧与缓冲区关联
//    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, out_buffer, target_format,
//                       video_decode_ctx->width, video_decode_ctx->height, 1);
    return 0;
}

// 把视频帧保存为GIF图片。save_index表示要把第几个视频帧保存为图片
// 这个警告不影响gif生成：No accelerated colorspace conversion found from yuv420p to bgr8
int save_gif_file(AVFrame *frame, int save_index) {
    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    av_log(NULL, AV_LOG_INFO, "format = %d, width = %d, height = %d\n",
                            frame->format, frame->width, frame->height);

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(gif_encode_ctx, frame);
    while (ret == 0) {
        packet->stream_index = 0;
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(gif_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, src_video->time_base, gif_stream->time_base);
        ret = av_write_frame(gif_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
    }
    av_packet_unref(packet); // 清除数据包
    return 0;
}

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
        // 转换器开始处理图像数据，把YUV图像转为RGB图像
        sws_scale(swsContext, (const uint8_t* const*) frame->data, frame->linesize,
            0, frame->height, rgb_frame->data, rgb_frame->linesize);
        rgb_frame->pts = frame->pts; // 设置数据帧的播放时间戳
        save_gif_file(rgb_frame, save_index); // 把视频帧保存为GIF图片
        break;
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    int save_index = 100;
    if (argc > 1) {
        src_name = argv[1];
    }
    if (argc > 2) {
        save_index = atoi(argv[2]);
    }
    char gif_name[20] = { 0 };
    sprintf(gif_name, "output_%03d.gif", save_index);
    if (open_input_file(src_name) < 0) { // 打开输入文件
        return -1;
    }
    if (open_output_file(gif_name) < 0) { // 打开输出文件
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "target image file is %s\n", gif_name);
    if (init_sws_context() < 0) { // 初始化图像转换器的实例
        return -1;
    }

    int ret = -1;
    int packet_index = -1; // 数据包的索引序号
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            packet_index++;
            decode_video(packet, frame, save_index); // 对视频帧解码
            if (packet_index > save_index) { // 已经采集到足够数量的帧
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_log(NULL, AV_LOG_INFO, "Success save %d_index frame as gif file.\n", save_index);
    
    av_write_trailer(gif_fmt_ctx); // 写入文件尾
    sws_freeContext(swsContext); // 释放图像转换器的实例
    avio_close(gif_fmt_ctx->pb); // 关闭输出流
    av_frame_free(&rgb_frame); // 释放数据帧资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avcodec_close(gif_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&gif_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(gif_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}