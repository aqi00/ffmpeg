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

AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
AVCodecContext *audio_decode_ctx = NULL; // 音频解码器的实例
int video_index = -1; // 视频流的索引
int audio_index = -1; // 音频流的索引
AVStream *src_video = NULL; // 源文件的视频流
AVStream *src_audio = NULL; // 源文件的音频流
AVStream *dest_audio = NULL; // 目标文件的音频流
AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
AVCodecContext *audio_encode_ctx = NULL; // 音频编码器的实例

AVFilterContext *abuffersrc_ctx = NULL; // 输入滤镜的实例
AVFilterContext *abuffersink_ctx = NULL; // 输出滤镜的实例
AVFilterGraph *afilter_graph = NULL; // 滤镜图

// 提取滤镜的名称
char *get_filter_name(const char *filters_desc) {
    char *ptr = NULL;
    int len = strlen(filters_desc);
    char temp[len+1];
    sprintf(temp, "%s", filters_desc);
    char *value = strtok(temp, "=");
    av_log(NULL, AV_LOG_INFO, "find filter name: %s\n", value);
    if (value) {
        size_t len = strlen(value) + 1;
        ptr = (char *) av_realloc(NULL, len);
        if (ptr)
            memcpy(ptr, value, len);
    }
    return ptr;
}

// 提取文件的扩展名
const char *get_file_ext(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    } else {
        return dot + 1;
    }
}

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
    }
    // 找到音频流的索引
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0) {
        src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec*) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "audio_decode_ctx is null\n");
            return -1;
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        ret = avcodec_open2(audio_decode_ctx, audio_codec, NULL); // 打开解码器的实例
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
    if (audio_index >= 0) { // 创建编码器实例和新的音频流
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找音频编码器
        AVCodec *audio_codec = (AVCodec*) avcodec_find_encoder(audio_codec_id);
        if (!audio_codec) {
            av_log(NULL, AV_LOG_ERROR, "audio_codec not found\n");
            return -1;
        }
        audio_encode_ctx = avcodec_alloc_context3(audio_codec); // 分配编码器的实例
        if (!audio_encode_ctx) {
            av_log(NULL, AV_LOG_ERROR, "audio_encode_ctx is null\n");
            return -1;
        }
        audio_encode_ctx->time_base = av_buffersink_get_time_base(abuffersink_ctx); // 时间基
        // 采样格式
        audio_encode_ctx->sample_fmt = (enum AVSampleFormat) av_buffersink_get_format(abuffersink_ctx);
        // 采样率，单位赫兹每秒
        audio_encode_ctx->sample_rate = av_buffersink_get_sample_rate(abuffersink_ctx);
        av_buffersink_get_ch_layout(abuffersink_ctx, &audio_encode_ctx->ch_layout); // 声道布局
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
    const AVFilter *buffersrc = avfilter_get_by_name("abuffer"); // 获取输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("abuffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    AVFilterInOut *outputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    afilter_graph = avfilter_graph_alloc(); // 分配一个滤镜图
    if (!outputs || !inputs || !afilter_graph) {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    char ch_layout[128];
    av_channel_layout_describe(&audio_decode_ctx->ch_layout, ch_layout, sizeof(ch_layout));
    int nb_channels = audio_decode_ctx->ch_layout.nb_channels;
    char args[512]; // 临时字符串，存放输入源的媒体参数信息，比如音频的采样率、采样格式等
    snprintf(args, sizeof(args),
        "sample_rate=%d:sample_fmt=%s:channel_layout=%s:channels=%d:time_base=%d/%d",
        audio_decode_ctx->sample_rate, av_get_sample_fmt_name(audio_decode_ctx->sample_fmt), 
        ch_layout, nb_channels,
        audio_decode_ctx->time_base.num, audio_decode_ctx->time_base.den);
    av_log(NULL, AV_LOG_INFO, "args : %s\n", args);
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&abuffersrc_ctx, buffersrc, "in",
        args, NULL, afilter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        return ret;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&abuffersink_ctx, buffersink, "out",
        NULL, NULL, afilter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }
    // atempo滤镜要求提前设置sample_fmts，否则av_buffersink_get_format得到的格式不对，会报错Specified sample format flt is invalid or not supported
    enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
    // 将二进制选项设置为整数列表，此处给输出滤镜的实例设置采样格式
    ret = av_opt_set_int_list(abuffersink_ctx, "sample_fmts", sample_fmts,
        AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        return ret;
    }

    // 设置滤镜的输入输出参数
    outputs->name = av_strdup("in");
    outputs->filter_ctx = abuffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = abuffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    // 把采用过滤字符串描述的图形添加到滤镜图
    ret = avfilter_graph_parse_ptr(afilter_graph, filters_desc, &inputs, &outputs, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot parse graph string\n");
        return ret;
    }
    // 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式
    ret = avfilter_graph_config(afilter_graph, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot config filter graph\n");
        return ret;
    }
    avfilter_inout_free(&inputs); // 释放滤镜的输入参数
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
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
        packet->stream_index = video_index>=0?1:0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 对音频帧重新编码
int recode_audio(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(audio_decode_ctx, packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(audio_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 把原始的数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(abuffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            break;
        }
        while (1) {
            // 从输出滤镜的接收器获取一个已加工的过滤帧
            ret = av_buffersink_get_frame(abuffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                break;
            }
            output_audio(filt_frame); // 给音频帧编码，并写入压缩后的音频包
        }
    }
    return ret;
}

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    const char *filters_desc = "";
    if (argc > 1) {
        src_name = argv[1];
    }
    if (argc > 2) {
        filters_desc = argv[2]; // 过滤字符串从命令行读取
    } else {
        av_log(NULL, AV_LOG_ERROR, "please enter command such as:\n  ./audiofilter src_name filters_desc\n");
        return -1;
    }
    const char *filter_name = get_filter_name(filters_desc);
    const char *ext_name = get_file_ext(src_name);
    char dest_name[64];
    sprintf(dest_name, "output_%s.%s", filter_name, ext_name);
    if (open_input_file(src_name) < 0) { // 打开输入文件
        return -1;
    }
    init_filter(filters_desc); // 初始化滤镜
    if (open_output_file(dest_name) < 0) { // 打开输出文件
        return -1;
    }
    // 修改音频速率的话，要考虑视频速率是否也跟着变化。atempo表示调整音频播放速度
    int is_atempo = (strstr(filters_desc, "atempo=") != NULL);
    
    int ret = -1;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    AVFrame *filt_frame = av_frame_alloc(); // 分配一个过滤后的数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要重新编码
            packet->stream_index = video_index>=0?1:0;
            recode_audio(packet, frame, filt_frame); // 对音频帧重新编码
        } else if (packet->stream_index == video_index && !is_atempo) {
            packet->stream_index = 0;
            // 视频包暂不重新编码，直接写入目标文件
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "write frame occur error %d.\n", ret);
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_audio(packet, frame, filt_frame); // 对音频帧重新编码
    output_audio(NULL); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success process audio file.\n");
    
    avfilter_free(abuffersrc_ctx); // 释放输入滤镜的实例
    avfilter_free(abuffersink_ctx); // 释放输出滤镜的实例
    avfilter_graph_free(&afilter_graph); // 释放滤镜图资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avcodec_close(audio_encode_ctx); // 关闭音频编码器的实例
    avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}
