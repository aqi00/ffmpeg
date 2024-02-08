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

AVFormatContext *in_fmt_ctx[2] = {NULL, NULL}; // 输入文件的封装器实例
AVCodecContext *image_decode_ctx[2] = {NULL, NULL}; // 图像解码器的实例
int video_index[2] = {-1 -1}; // 视频流的索引
AVStream *src_video = NULL; // 源文件的视频流
AVStream *dest_video = NULL; // 目标文件的视频流
AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
AVCodecContext *video_encode_ctx = NULL; // 视频编码器的实例
struct SwsContext *swsContext[2] = {NULL, NULL}; // 图像转换器的实例
AVFrame *yuv_frame[2] = {NULL, NULL}; // YUV数据帧
int packet_index = 0; // 数据帧的索引序号

// 打开输入文件
int open_input_file(int seq, const char *src_name) {
    // 打开图像文件
    int ret = avformat_open_input(&in_fmt_ctx[seq], src_name, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找图像文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx[seq], NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到视频流的索引
    video_index[seq] = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index[seq] >= 0) {
        AVStream *src_video = in_fmt_ctx[seq]->streams[video_index[seq]];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找图像解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        image_decode_ctx[seq] = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!image_decode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "image_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(image_decode_ctx[seq], src_video->codecpar);
        ret = avcodec_open2(image_decode_ctx[seq], video_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open image_decode_ctx.\n");
            return -1;
        }
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }
    return 0;
}

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
    if (video_index[0] >= 0) { // 创建编码器实例和新的视频流
        src_video = in_fmt_ctx[0]->streams[video_index[0]];
        // 查找视频编码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!video_codec) {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "video_encode_ctx is null\n");
            return -1;
        }
        video_encode_ctx->pix_fmt = AV_PIX_FMT_YUV420P; // 像素格式
        video_encode_ctx->width = src_video->codecpar->width; // 视频宽度
        video_encode_ctx->height = src_video->codecpar->height; // 视频高度
        video_encode_ctx->framerate = (AVRational){25, 1}; // 帧率
        video_encode_ctx->time_base = (AVRational){1, 25}; // 时间基
        video_encode_ctx->gop_size = 12; // 关键帧的间隔距离
//        video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_open2(video_encode_ctx, video_codec, NULL); // 打开编码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_encode_ctx.\n");
            return -1;
        }
        dest_video = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把编码器实例的参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        dest_video->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");
    return 0;
}

// 初始化图像转换器的实例
int init_sws_context(int seq) {
    enum AVPixelFormat target_format = AV_PIX_FMT_YUV420P; // 视频的像素格式是YUV
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    swsContext[seq] = sws_getContext(
            image_decode_ctx[seq]->width, image_decode_ctx[seq]->height, image_decode_ctx[seq]->pix_fmt, 
            video_encode_ctx->width, video_encode_ctx->height, target_format, 
            SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (swsContext == NULL) {
        av_log(NULL, AV_LOG_ERROR, "swsContext is null\n");
        return -1;
    }
    yuv_frame[seq] = av_frame_alloc(); // 分配一个YUV数据帧
    yuv_frame[seq]->format = target_format; // 像素格式
    yuv_frame[seq]->width = video_encode_ctx->width; // 视频宽度
    yuv_frame[seq]->height = video_encode_ctx->height; // 视频高度
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(yuv_frame[seq]->data, yuv_frame[seq]->linesize, 
            video_encode_ctx->width, video_encode_ctx->height, target_format, 1);
//    // 分配缓冲区空间，用于存放转换后的图像数据
//    int buffer_size = av_image_get_buffer_size(target_format, video_encode_ctx->width, video_encode_ctx->height, 1);
//    unsigned char *out_buffer = (unsigned char*)av_malloc(
//                            (size_t)buffer_size * sizeof(unsigned char));
//    // 将数据帧与缓冲区关联
//    av_image_fill_arrays(yuv_frame[seq]->data, yuv_frame[seq]->linesize, out_buffer,
//                       target_format, video_encode_ctx->width, video_encode_ctx->height, 1);
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
        av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
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

// 对视频帧重新编码
int recode_video(int seq, AVPacket *packet, AVFrame *frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(image_decode_ctx[seq], packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(image_decode_ctx[seq], frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 转换器开始处理图像数据，把RGB图像转为YUV图像
        sws_scale(swsContext[seq], (const uint8_t* const*) frame->data, frame->linesize,
            0, frame->height, yuv_frame[seq]->data, yuv_frame[seq]->linesize);
        int i = 0;
        while (i++ < 100) { // 每张图片占据100个视频帧
            yuv_frame[seq]->pts = packet_index++; // 播放时间戳要递增
            output_video(yuv_frame[seq]); // 给视频帧编码，并写入压缩后的视频包
        }
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *src_name0 = "../input.jpg";
    const char *src_name1 = "../input.png";
    const char *dest_name = "output_image2video.mp4";
    if (argc > 1) {
        src_name0 = argv[1];
    }
    if (argc > 2) {
        src_name1 = argv[2];
    }
    if (argc > 3) {
        dest_name = argv[3];
    }
    if (open_input_file(0, src_name0) < 0) { // 打开第一个图片文件
        return -1;
    }
    if (open_input_file(1, src_name1) < 0) { // 打开第二个图片文件
        return -1;
    }
    if (open_output_file(dest_name) < 0) { // 打开输出文件
        return -1;
    }
    if (init_sws_context(0) < 0) { // 初始化第一个图像转换器的实例
        return -1;
    }
    if (init_sws_context(1) < 0) { // 初始化第二个图像转换器的实例
        return -1;
    }
    
    int ret = -1;
    // 首先把第一张图片转为视频文件
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx[0], packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index[0]) { // 视频包需要重新编码
            recode_video(0, packet, frame); // 对视频帧重新编码
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(0, packet, frame); // 对视频帧重新编码
    // 然后在视频末尾追加第二张图片
    while (av_read_frame(in_fmt_ctx[1], packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index[1]) { // 视频包需要重新编码
            recode_video(1, packet, frame); // 对视频帧重新编码
        }
        av_packet_unref(packet); // 清除数据包
    }
//    yuv_frame[1]->pts = packet_index++; // 播放时间戳要递增
//    output_video(yuv_frame[1]); // 末尾补上一帧，避免尾巴丢帧
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(1, packet, frame); // 对视频帧重新编码
    output_video(NULL); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success convert image to video.\n");
    
    av_frame_free(&yuv_frame[0]); // 释放数据帧资源
    av_frame_free(&yuv_frame[1]); // 释放数据帧资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(image_decode_ctx[0]); // 关闭视频解码器的实例
    avcodec_free_context(&image_decode_ctx[0]); // 释放视频解码器的实例
    avcodec_close(image_decode_ctx[1]); // 关闭视频解码器的实例
    avcodec_free_context(&image_decode_ctx[1]); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    sws_freeContext(swsContext[0]); // 释放图像转换器的实例
    sws_freeContext(swsContext[1]); // 释放图像转换器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx[0]); // 关闭音视频文件
    avformat_close_input(&in_fmt_ctx[1]); // 关闭音视频文件
    return 0;
}