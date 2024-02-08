#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#ifdef __cplusplus
};
#endif

AVFormatContext *in_fmt_ctx[2] = {NULL, NULL}; // 输入文件的封装器实例
AVCodecContext *audio_decode_ctx[2] = {NULL, NULL}; // 音频解码器的实例
int video_index = -1; // 视频流的索引
int audio_index[2] = {-1, -1}; // 音频流的索引
AVStream *src_video = NULL; // 源文件的视频流
AVStream *src_audio[2] = {NULL, NULL}; // 源文件的音频流
AVStream *dest_audio = NULL; // 目标文件的音频流
AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
AVCodecContext *audio_encode_ctx = NULL; // 音频编码器的实例

AVFilterContext *buffersrc_ctx[2] = {NULL, NULL}; // 输入滤镜的实例
AVFilterContext *buffersink_ctx = NULL; // 输出滤镜的实例
AVFilterGraph *filter_graph = NULL; // 滤镜图

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
    if (seq == 0) {
        // 找到视频流的索引
        video_index = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (video_index >= 0) {
            src_video = in_fmt_ctx[seq]->streams[video_index];
        }
    }
    // 找到音频流的索引
    audio_index[seq] = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index[seq] >= 0) {
        src_audio[seq] = in_fmt_ctx[seq]->streams[audio_index[seq]];
        enum AVCodecID audio_codec_id = src_audio[seq]->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec*) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx[seq] = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx[seq]) {
            av_log(NULL, AV_LOG_ERROR, "audio_decode_ctx is null\n");
            return -1;
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx[seq], src_audio[seq]->codecpar);
        ret = avcodec_open2(audio_decode_ctx[seq], audio_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open audio_decode_ctx.\n");
            return -1;
        }
    } else {
        av_log(NULL, AV_LOG_ERROR, "Can't find audio stream.\n");
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
    if (video_index >= 0) { // 源文件有视频流，就给目标文件创建视频流
        AVStream *dest_video = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的视频参数原样复制过来
        avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
        dest_video->time_base = src_video->time_base;
        dest_video->codecpar->codec_tag = 0;
    }
    if (audio_index[0] >= 0) { // 创建编码器实例和新的音频流
        // 查找音频编码器
        AVCodec *audio_codec = (AVCodec*) avcodec_find_encoder(src_audio[0]->codecpar->codec_id);
        if (!audio_codec) {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_encode_ctx = avcodec_alloc_context3(audio_codec); // 分配编码器的实例
        if (!audio_encode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "audio_encode_ctx is null\n");
            return -1;
        }
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_to_context(audio_encode_ctx, src_audio[0]->codecpar);
        audio_encode_ctx->time_base = src_audio[0]->time_base; // 时间基
        ret = avcodec_open2(audio_encode_ctx, audio_codec, NULL); // 打开编码器的实例
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't open audio_encode_ctx.\n");
            return -1;
        }
        dest_audio = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把编码器实例的参数复制给目标音频流
        avcodec_parameters_from_context(dest_audio->codecpar, audio_encode_ctx);
        dest_audio->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "write file_header occur error %d.\n", ret);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success write file_header.\n");
    return 0;
}

// 初始化滤镜（也称过滤器、滤波器）
int init_filter(const char *filters_desc) {
    av_log(NULL, AV_LOG_INFO, "filters_desc : %s\n", filters_desc);
    int ret = 0;
    const AVFilter *buffersrc[2];
    buffersrc[0] = avfilter_get_by_name("abuffer"); // 获取第一个输入滤镜
    buffersrc[1] = avfilter_get_by_name("abuffer"); // 获取第二个输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("abuffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    AVFilterInOut *outputs[2];
    outputs[0] = avfilter_inout_alloc(); // 分配第一个滤镜的输入输出参数
    outputs[1] = avfilter_inout_alloc(); // 分配第二个滤镜的输入输出参数
    filter_graph = avfilter_graph_alloc(); // 分配一个滤镜图
    if (!inputs || !outputs[0] || !outputs[1] || !filter_graph) {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    char ch_layout0[128];
    av_channel_layout_describe(&audio_decode_ctx[0]->ch_layout, ch_layout0, sizeof(ch_layout0));
    int nb_channels0 = audio_decode_ctx[0]->ch_layout.nb_channels;
    char args0[512]; // 临时字符串，存放输入源的媒体参数信息，比如音频的采样率、采样格式等
    snprintf(args0, sizeof(args0),
        "sample_rate=%d:sample_fmt=%s:channel_layout=%s:channels=%d:time_base=%d/%d",
        audio_decode_ctx[0]->sample_rate, av_get_sample_fmt_name(audio_decode_ctx[0]->sample_fmt), 
        ch_layout0, nb_channels0,
        audio_decode_ctx[0]->time_base.num, audio_decode_ctx[0]->time_base.den);
    av_log(NULL, AV_LOG_INFO, "args0 = %s\n", args0);
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx[0], buffersrc[0], "in0",
        args0, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer0 source\n");
        return ret;
    }
    char ch_layout1[128];
    av_channel_layout_describe(&audio_decode_ctx[1]->ch_layout, ch_layout1, sizeof(ch_layout1));
    int nb_channels1 = audio_decode_ctx[1]->ch_layout.nb_channels;
    char args1[512]; // 临时字符串，存放输入源的媒体参数信息，比如音频的采样率、采样格式等
    snprintf(args1, sizeof(args1),
        "sample_rate=%d:sample_fmt=%s:channel_layout=%s:channels=%d:time_base=%d/%d",
        audio_decode_ctx[1]->sample_rate, av_get_sample_fmt_name(audio_decode_ctx[1]->sample_fmt), 
        ch_layout1, nb_channels1,
        audio_decode_ctx[1]->time_base.num, audio_decode_ctx[1]->time_base.den);
    av_log(NULL, AV_LOG_INFO, "args1 = %s\n", args1);
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx[1], buffersrc[1], "in1",
        args1, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer1 source\n");
        return ret;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
        NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }
    // atempo滤镜要求提前设置sample_fmts，否则av_buffersink_get_format得到的格式不对，会报错Specified sample format flt is invalid or not supported
    enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
    // 将二进制选项设置为整数列表，此处给输出滤镜的实例设置采样格式
    ret = av_opt_set_int_list(buffersink_ctx, "sample_fmts", sample_fmts,
        AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        return ret;
    }

    // 设置滤镜的输入输出参数
    outputs[0]->name = av_strdup("0:a"); // 第一路音频流
    outputs[0]->filter_ctx = buffersrc_ctx[0];
    outputs[0]->pad_idx = 0;
    outputs[0]->next = outputs[1]; // 注意这里要指向下一个输入输出参数
    outputs[1]->name = av_strdup("1:a"); // 第二路音频流
    outputs[1]->filter_ctx = buffersrc_ctx[1];
    outputs[1]->pad_idx = 0;
    outputs[1]->next = NULL;
    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    // 把采用过滤字符串描述的图形添加到滤镜图（引脚的输出和输入与滤镜容器的相反）
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, outputs, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot parse graph string\n");
        return ret;
    }
    // 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式
    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }
    avfilter_inout_free(&inputs); // 释放滤镜的输入参数
    avfilter_inout_free(outputs); // 释放滤镜的输出参数
    av_log(NULL, AV_LOG_INFO, "Success initialize filter.\n");
    return ret;
}

// 给音频帧编码，并写入压缩后的音频包
int output_audio(AVFrame *frame) {
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(audio_encode_ctx, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send frame occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        AVPacket *packet = av_packet_alloc(); // 分配一个数据包
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(audio_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "encode frame occur error %d.\n", ret);
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, audio_encode_ctx->time_base, dest_audio->time_base);
        packet->stream_index = 1;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 从指定的输入文件获取一个数据帧
int get_frame(AVFormatContext *fmt_ctx, AVCodecContext *decode_ctx, int index, AVPacket *packet, AVFrame *frame) {
    int ret = 0;
    while ((ret = av_read_frame(fmt_ctx, packet)) >= 0) { // 轮询数据包
        if (packet->stream_index == index) {
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0) {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                } else if (ret < 0) {
                    continue;
                }
            }
            break;
        }
    }
    av_packet_unref(packet); // 清除数据包
    return ret;
}

// 对音频帧重新编码
int recode_audio(AVPacket **packet, AVFrame **frame, AVFrame *filt_frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(audio_decode_ctx[0], packet[0]);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(audio_decode_ctx[0], frame[0]);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            return ret;
        }
        // 把第一个文件的数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx[0], frame[0], AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            return ret;
        }
        // 从指定的输入文件获取一个数据帧
        ret = get_frame(in_fmt_ctx[1], audio_decode_ctx[1], audio_index[1], packet[1], frame[1]);
        if (ret == 0) { // 第二个文件没到末尾，就把数据帧添加到输入滤镜的缓冲区
            ret = av_buffersrc_add_frame_flags(buffersrc_ctx[1], frame[1], AV_BUFFERSRC_FLAG_KEEP_REF);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                return ret;
            }
        } else { // 第二个文件已到末尾，就把空白帧添加到输入滤镜的缓冲区
            ret = av_buffersrc_add_frame_flags(buffersrc_ctx[1], NULL, AV_BUFFERSRC_FLAG_KEEP_REF);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                return ret;
            }
        }
        while (1) {
            // 从输出滤镜的接收器获取一个已加工的过滤帧
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return ret;
            } else if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                return ret;
            }
            output_audio(filt_frame); // 给音频帧编码，并写入压缩后的音频包
        }
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *src_name0 = "../fuzhou.mp4";
    const char *src_name1 = "../test.mp3";
    const char *dest_name = "output_background.mp4";
    const char *filters_desc = "";
    if (argc > 1) {
        src_name0 = argv[1];
    }
    if (argc > 2) {
        src_name1 = argv[2];
    }
    if (argc > 3) {
        filters_desc = argv[3]; // 过滤字符串从命令行读取
    } else {
        av_log(NULL, AV_LOG_ERROR, "please enter command such as:\n  ./mixaudio src_name0 src_name1 filters_desc\n");
        return -1;
    }
    if (open_input_file(0, src_name0) < 0) { // 打开第一个输入文件
        return -1;
    }
    if (open_input_file(1, src_name1) < 0) { // 打开第二个输入文件
        return -1;
    }
    init_filter(filters_desc); // 初始化滤镜
    if (open_output_file(dest_name) < 0) { // 打开输出文件
        return -1;
    }
    
    int ret = -1;
    AVPacket *packet[2];
    packet[0] = av_packet_alloc(); // 分配一个数据包
    packet[1] = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame[2];
    frame[0] = av_frame_alloc(); // 分配一个数据帧
    frame[1] = av_frame_alloc(); // 分配一个数据帧
    AVFrame *filt_frame = av_frame_alloc(); // 分配一个过滤后的数据帧
    while (av_read_frame(in_fmt_ctx[0], packet[0]) >= 0) { // 轮询数据包
        if (packet[0]->stream_index == video_index) { // 视频包无需重新编码，直接写入
            packet[0]->stream_index = 0;
            ret = av_write_frame(out_fmt_ctx, packet[0]); // 往文件写入一个数据包
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
                break;
            }
        } else if (packet[0]->stream_index == audio_index[0]) { // 音频包需要重新编码
            packet[0]->stream_index = 1;
            recode_audio(packet, frame, filt_frame); // 对音频帧重新编码
        }
        av_packet_unref(packet[0]); // 清除数据包
    }
    output_audio(NULL); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success add background audio.\n");
    
    avfilter_free(buffersrc_ctx[0]); // 释放输入滤镜的实例
    avfilter_free(buffersrc_ctx[1]); // 释放输入滤镜的实例
    avfilter_free(buffersink_ctx); // 释放输出滤镜的实例
    avfilter_graph_free(&filter_graph); // 释放滤镜图资源
    av_frame_free(&frame[0]); // 释放数据帧资源
    av_frame_free(&frame[1]); // 释放数据帧资源
    av_frame_free(&filt_frame); // 释放数据帧资源
    av_packet_free(&packet[0]); // 释放数据包资源
    av_packet_free(&packet[1]); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(audio_decode_ctx[0]); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx[0]); // 释放音频解码器的实例
    avcodec_close(audio_decode_ctx[1]); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx[1]); // 释放音频解码器的实例
    avcodec_close(audio_encode_ctx); // 关闭音频编码器的实例
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx[0]); // 关闭音视频文件
    avformat_close_input(&in_fmt_ctx[1]); // 关闭音视频文件
    return 0;
}
