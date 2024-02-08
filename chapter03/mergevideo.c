#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#ifdef __cplusplus
};
#endif

AVFormatContext *in_fmt_ctx[2] = {NULL, NULL}; // 输入文件的封装器实例
AVCodecContext *video_decode_ctx[2] = {NULL, NULL}; // 视频解码器的实例
int video_index[2] = {-1 -1}; // 视频流的索引
int audio_index[2] = {-1 -1}; // 音频流的索引
AVStream *src_video[2] = {NULL, NULL}; // 源文件的视频流
AVStream *dest_video = NULL; // 目标文件的视频流
AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
AVCodecContext *video_encode_ctx = NULL; // 视频编码器的实例

// 打开输入文件
int open_input_file(int seq, const char *src_name) {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx[seq], src_name, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", src_name);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx[seq], NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    // 找到视频流的索引
    video_index[seq] = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index[seq] >= 0) {
        src_video[seq] = in_fmt_ctx[seq]->streams[video_index[seq]];
        enum AVCodecID video_codec_id = src_video[seq]->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_decode_ctx[seq] = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "video_decode_ctx is null\n");
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx[seq], src_video[seq]->codecpar);
        ret = avcodec_open2(video_decode_ctx[seq], video_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open video_decode_ctx.\n");
            return -1;
        }
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream.\n");
        return -1;
    }
    // 找到音频流的索引
    audio_index[seq] = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
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
        AVStream *src_video = in_fmt_ctx[0]->streams[video_index[0]];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频编码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_encoder(video_codec_id);
        if (!video_codec) {
            av_log(NULL, AV_LOG_ERROR, "video_codec not found\n");
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "video_encode_ctx is null\n");
            return -1;
        }
        // 把源视频流中的编解码参数复制给编码器的实例
        avcodec_parameters_to_context(video_encode_ctx, src_video->codecpar);
        // 注意：帧率和时间基要单独赋值，因为avcodec_parameters_to_context没复制这两个参数
        video_encode_ctx->framerate = src_video->r_frame_rate;
        // framerate.num值过大，会导致视频头一秒变灰色
        if (video_encode_ctx->framerate.num > 60) {
            video_encode_ctx->framerate = (AVRational){25, 1}; // 帧率
        }
        video_encode_ctx->time_base = src_video->time_base;
        video_encode_ctx->gop_size = 12; // 关键帧的间隔距离
        //video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
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
        // 如果后面有对视频帧转换时间基，这里就无需复制时间基
        //dest_video->time_base = src_video->time_base;
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
        av_packet_rescale_ts(packet, src_video[0]->time_base, dest_video->time_base);
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
int recode_video(int seq, AVPacket *packet, AVFrame *frame, int64_t begin_video_pts) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx[seq], packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx[seq], frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        if (seq == 1) { // 第二个输入文件
            // 把时间戳从一个时间基改为另一个时间基
            int64_t pts = av_rescale_q(frame->pts, src_video[1]->time_base, src_video[0]->time_base);
            frame->pts = pts + begin_video_pts; // 加上增量时间戳
        }
        output_video(frame); // 给视频帧编码，并写入压缩后的视频包
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *src_name0 = "../fuzhous.mp4";
    const char *src_name1 = "../seas.mp4";
    const char *dest_name = "output_mergevideo.mp4";
    if (argc > 1) {
        src_name0 = argv[1];
    }
    if (argc > 2) {
        src_name1 = argv[2];
    }
    if (argc > 3) {
        dest_name = argv[3];
    }
    if (open_input_file(0, src_name0) < 0) { // 打开第一个输入文件
        return -1;
    }
    if (open_input_file(1, src_name1) < 0) { // 打开第二个输入文件
        return -1;
    }
    if (open_output_file(dest_name) < 0) { // 打开输出文件
        return -1;
    }
    
    int ret = -1;
    // 首先原样复制第一个视频
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx[0], packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index[0]) { // 视频包需要重新编码
            recode_video(0, packet, frame, 0); // 对视频帧重新编码
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(0, packet, frame, 0); // 对视频帧重新编码
    // 然后在末尾追加第二个视频
    int64_t begin_sec = in_fmt_ctx[0]->duration; // 获取视频时长，单位微秒
        // 计算第一个视频末尾的时间基，作为第二个视频开头的时间基
        int64_t begin_video_pts = begin_sec / ( 1000.0 * 1000.0 * av_q2d(src_video[0]->time_base));
    while (av_read_frame(in_fmt_ctx[1], packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index[1]) { // 视频包需要重新编码
            recode_video(1, packet, frame, begin_video_pts); // 对视频帧重新编码
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(1, packet, frame, begin_video_pts); // 对视频帧重新编码
    output_video(NULL); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success merge two video file.\n");
    
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(video_decode_ctx[0]); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx[0]); // 释放视频解码器的实例
    avcodec_close(video_decode_ctx[1]); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx[1]); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx[0]); // 关闭音视频文件
    avformat_close_input(&in_fmt_ctx[1]); // 关闭音视频文件
    return 0;
}