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
    const char *filename = "output.mp4";
    if (argc > 1) {
        filename = argv[1];
    }
    AVFormatContext *out_fmt_ctx;
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, filename);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't alloc output_file %s.\n", filename);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open output_file %s.\n", filename);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open output_file %s.\n", filename);
    
    // 查找编码器
    AVCodec *video_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec) {
        av_log(NULL, AV_LOG_ERROR, "AV_CODEC_ID_H264 not found\n");
        return -1;
    }
    AVCodecContext *video_encode_ctx = NULL;
    video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编解码器的实例
    if (!video_encode_ctx) {
        av_log(NULL, AV_LOG_ERROR, "video_encode_ctx is null\n");
        return -1;
    }
    video_encode_ctx->width = 320; // 视频画面的宽度
    video_encode_ctx->height = 240; // 视频画面的高度
    // 创建指定编码器的数据流
    AVStream * video_stream = avformat_new_stream(out_fmt_ctx, video_codec);
    // 把编码器实例中的参数复制给数据流
    avcodec_parameters_from_context(video_stream->codecpar, video_encode_ctx);
    video_stream->codecpar->codec_tag = 0; // 非特殊情况都填0
    
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");
    av_write_trailer(out_fmt_ctx); // 写文件尾
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    return 0;
}
