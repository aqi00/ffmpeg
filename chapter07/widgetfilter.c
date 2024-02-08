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
AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例
int video_index = -1; // 视频流的索引
int audio_index = -1; // 音频流的索引
AVStream *src_video = NULL; // 源文件的视频流
AVStream *src_audio = NULL; // 源文件的音频流
AVStream *dest_video = NULL; // 目标文件的视频流
AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
AVCodecContext *video_encode_ctx = NULL; // 视频编码器的实例

AVFilterContext *buffersrc_ctx = NULL; // 输入滤镜的实例
AVFilterContext *buffersink_ctx = NULL; // 输出滤镜的实例
AVFilterGraph *filter_graph = NULL; // 滤镜图

#include <iconv.h> // iconv用于字符内码转换

int pre_num(unsigned char byte) {
    unsigned char mask = 0x80;
    int num = 0;
    int i = 0;
    while (i++ < 8) {
        if ((byte & mask) == mask) {
            mask = mask >> 1;
            num++;
        } else {
            break;
        }
    }
    return num;
}

// 是否为UTF-8编码
int is_utf8(unsigned char *data, int len) {
    int num = 0;
    int i = 0;
    while (i < len) {
        if ((data[i] & 0x80) == 0x00) {
            i++;
            continue;
        } else if ((num = pre_num(data[i])) > 2) {
            i++;
            int j = 0;
            while (j++ < num-1) {
                // 判断后面num - 1 个字节是不是都是10开
                if ((data[i] & 0xc0) != 0x80) {
                    return -1;
                }
                i++;
            }
        } else {
            // 其他情况说明不是utf-8
            return -1;
        }
    }
    return 0;
}

// “流”的i+1编码为0xf7。GB2312的汉字编码范围是0xB0A1～0xF7FE，GBK的汉字编码范围是0x8140～0xFEFE，且剔除0x7F码位
// 是否为GBK编码
int is_gbk(unsigned char *data, int len) {
    int i = 0;
    while (i < len) {
        if (data[i] <= 0x7f) {
            // 编码小于等于127，只有一个字节的编码，兼容ASCII
            i++;
            continue;
        } else {
            // 大于127的使用双字节编码
            if (data[i] >= 0x81 &&
                data[i] <= 0xfe &&
                data[i + 1] >= 0x40 &&
                data[i + 1] <= 0xfe) {
                i += 2;
                continue;
            } else {
                return -1;
            }
        }
    }
    return 0;
}

// 是否为ASCII编码
int is_ascii(unsigned char *data, int len) {
    int i = 0;
    while (i < len) {
        if (data[i] <= 0x7f) {
            // 编码小于等于127,只有一个字节的编码，兼容ASCII
            i++;
            continue;
        } else {
            return -1;
        }
    }
    return 0;
}

// 把字符串从GBK编码改为UTF-8编码
int gbk_to_utf8(char *src_str, size_t src_len, char *dst_str, size_t dst_len) {
    iconv_t cd;
    char **pin = &src_str;
    char **pout = &dst_str;
    cd = iconv_open("UTF-8", "GBK");
    if (cd == NULL)
        return -1;
    memset(dst_str, 0, dst_len);
    if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
        return -1;
    iconv_close(cd);
    *pout[0] = '\0';
    return 0;
}

// 把字符串从UTF-8编码改为GBK编码
int utf8_to_gbk(char *src_str, size_t src_len, char *dst_str, size_t dst_len) {
    iconv_t cd;
    char **pin = &src_str;
    char **pout = &dst_str;
    cd = iconv_open("GBK", "UTF-8");
    if (cd == NULL)
        return -1;
    memset(dst_str, 0, dst_len);
    if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
        return -1;
    iconv_close(cd);
    *pout[0] = '\0';
    return 0;
}

// 提取滤镜的名称
char *get_filter_name(const char *filters_desc) {
    char *ptr = NULL;
    int len_desc = strlen(filters_desc);
    char temp[len_desc+1];
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

// 替换字符串中的特定字符串
char *strrpl(char *s, const char *s1, const char *s2) {
    char *ptr;
    while (ptr = strstr(s, s1)) { // 如果在s中找到s1
        memmove(ptr + strlen(s2) , ptr + strlen(s1), strlen(ptr) - strlen(s1) + 1);
        memcpy(ptr, &s2[0], strlen(s2));
    }
    return s;
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
    // 找到音频流的索引
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0) {
        src_audio = in_fmt_ctx->streams[audio_index];
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
    if (video_index >= 0) { // 创建编码器实例和新的视频流
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
        video_encode_ctx->framerate = av_buffersink_get_frame_rate(buffersink_ctx); // 帧率
        video_encode_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx); // 时间基
        video_encode_ctx->gop_size = 12; // 关键帧的间隔距离
        video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx); // 视频宽度
        video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx); // 视频高度
        // 视频的像素格式（颜色空间）
        video_encode_ctx->pix_fmt = (enum AVPixelFormat) av_buffersink_get_format(buffersink_ctx);
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
        dest_video->codecpar->codec_tag = 0;
    }
    if (audio_index >= 0) { // 源文件有音频流，就给目标文件创建音频流
        AVStream *dest_audio = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
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
    const AVFilter *buffersrc = avfilter_get_by_name("buffer"); // 获取输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("buffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    AVFilterInOut *outputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    filter_graph = avfilter_graph_alloc(); // 分配一个滤镜图
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    char args[512]; // 临时字符串，存放输入源的媒体参数信息，比如视频的宽高、像素格式等
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        video_decode_ctx->width, video_decode_ctx->height, video_decode_ctx->pix_fmt,
        src_video->time_base.num, src_video->time_base.den,
        video_decode_ctx->sample_aspect_ratio.num, video_decode_ctx->sample_aspect_ratio.den);
    av_log(NULL, AV_LOG_INFO, "args : %s\n", args);
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
        args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        return ret;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
        NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return ret;
    }
    // 将二进制选项设置为整数列表，此处给输出滤镜的实例设置像素格式
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        return ret;
    }
    // 设置滤镜的输入输出参数
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    // 把采用过滤字符串描述的图形添加到滤镜图
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, &outputs, NULL);
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
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
    av_log(NULL, AV_LOG_INFO, "Success initialize filter.\n");
    return ret;
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

// 对视频帧重新编码
int recode_video(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame) {
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
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
            break;
        }
        // 把原始的数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            break;
        }
        while (1) {
            // 从输出滤镜的接收器获取一个已加工的过滤帧
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "get buffersink frame occur error %d.\n", ret);
                break;
            }
            output_video(filt_frame); // 给视频帧编码，并写入压缩后的视频包
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
        av_log(NULL, AV_LOG_ERROR, "please enter command such as:\n  ./widgetfilter src_name filters_desc\n");
        return -1;
    }
    if (open_input_file(src_name) < 0) { // 打开输入文件
        return -1;
    }
    int filters_len = strlen(filters_desc);
    // 如果中文字符采用GBK编码，就要把它转换为UTF-8编码
    if (is_gbk((unsigned char*)filters_desc, filters_len) == 0) {
        int dst_len = filters_len*3/2;
        char *dst_str = (char *) malloc(dst_len+1);
        // 把字符串从GBK编码改为UTF-8编码
        gbk_to_utf8((char *) filters_desc, filters_len, dst_str, dst_len);
        filters_desc = dst_str;
    }
    // 根据第一个滤镜名称构造输出文件的名称
    const char *filter_name = get_filter_name(filters_desc);
    char dest_name[64];
    sprintf(dest_name, "output_%s.mp4", filter_name);
    av_log(NULL, AV_LOG_INFO, "dest_name: %s\n", dest_name);
    // 下面把过滤字符串中的特定串替换为相应数值
    char total_frames[16];
    sprintf(total_frames, "%d", src_video->nb_frames);
    // start_frame可以使用算术表达式
    filters_desc = strrpl((char *)filters_desc, "TOTAL_FRAMES", total_frames);
    char duration[16];
    sprintf(duration, "%.2f", src_video->nb_frames/1000/1000.0);
    // start_time不能使用算术表达式
    filters_desc = strrpl((char *)filters_desc, "DURATION", duration);
    init_filter(filters_desc); // 初始化滤镜
    if (open_output_file(dest_name) < 0) { // 打开输出文件
        return -1;
    }
    
    int ret = -1;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    AVFrame *filt_frame = av_frame_alloc(); // 分配一个过滤后的数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            packet->stream_index = 0;
            recode_video(packet, frame, filt_frame); // 对视频帧重新编码
        } else if (packet->stream_index == audio_index) { // 音频包暂不重新编码，直接写入目标文件
            packet->stream_index = 1;
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
    recode_video(packet, frame, filt_frame); // 对视频帧重新编码
    output_video(NULL); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    av_log(NULL, AV_LOG_INFO, "Success process video file.\n");
    
    avfilter_free(buffersrc_ctx); // 释放输入滤镜的实例
    avfilter_free(buffersink_ctx); // 释放输出滤镜的实例
    avfilter_graph_free(&filter_graph); // 释放滤镜图资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return 0;
}
