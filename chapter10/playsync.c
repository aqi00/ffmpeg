#include <stdio.h>
#include <SDL.h>
// 引入SDL要增加下面的声明#undef main，否则编译会报错“undefined reference to `WinMain'”
#undef main

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif

#define MAX_AUDIO_FRAME_SIZE 8096 // 一帧音频最大长度（样本数），该值不能太小
int audio_len = 0; // 一帧PCM音频的数据长度
unsigned char *audio_pos = NULL; // 当前读取的位置

AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例
AVCodecContext *audio_decode_ctx = NULL; // 音频解码器的实例
int video_index = -1; // 视频流的索引
int audio_index = -1; // 音频流的索引
AVStream *src_video = NULL; // 源文件的视频流
AVStream *src_audio = NULL; // 源文件的音频流
SwrContext *swr_ctx = NULL; // 音频采样器的实例
struct SwsContext *swsContext = NULL; // 图像转换器的实例
AVFrame *sws_frame = NULL; // 临时转换的数据帧
enum AVPixelFormat target_format = AV_PIX_FMT_YUV420P; // 目标的像素格式
int target_width = 0; // 目标画面的宽度
int target_height = 0; // 目标画面的高度

SDL_Window *window; // 声明SDL窗口
SDL_Renderer *renderer; // 声明SDL渲染器
SDL_Texture *texture; // 声明SDL纹理
SDL_Rect rect; // 声明SDL渲染区域
SDL_Event event; // 声明SDL事件
AVFrame *video_frame = NULL; // 声明一个视频帧
int out_buffer_size; // 缓冲区的大小
unsigned char *out_buff; // 缓冲区的位置

typedef struct PacketGroup {
    AVPacket packet; // 当前的数据包
    struct PacketGroup *next; // 下一个数据包组合
} PacketGroup; // 定义数据包组合结构

typedef struct PacketQueue {
    PacketGroup *first_pkt; // 第一个数据包组合
    PacketGroup *last_pkt; // 最后一个数据包组合
} PacketQueue; // 定义数据包队列结构

int interval; // 视频帧之间的播放间隔
int play_video = 0; // 是否正在播放视频
int is_close = 0; // 是否关闭窗口
int has_audio = 0; // 是否拥有音频流
double audio_time = 0; // 音频时钟，当前音频包对应的时间值
double video_time = 0; // 视频时钟，当前视频包对应的时间值
SDL_mutex *list_lock = NULL; // 声明一个队列锁，防止线程间同时操作包队列
SDL_mutex *frame_lock = NULL; // 声明一个帧锁，防止线程间同时操作视频帧
SDL_Thread *sdl_thread = NULL; // 声明一个SDL线程
PacketQueue packet_list; // 存放视频包的队列

// 数据包入列
void push_packet(AVPacket packet) {
    PacketGroup *this_pkt = (PacketGroup *) av_malloc(sizeof(PacketGroup));
    this_pkt->packet = packet;
    this_pkt->next = NULL;
    if (packet_list.first_pkt == NULL) {
        packet_list.first_pkt = this_pkt;
    }
    if (packet_list.last_pkt == NULL) {
        PacketGroup *last_pkt = (PacketGroup *) av_malloc(sizeof(PacketGroup));
        packet_list.last_pkt = last_pkt;
    }
    packet_list.last_pkt->next = this_pkt;
    packet_list.last_pkt = this_pkt;
    return;
}

// 数据包出列
AVPacket pop_packet() {
    PacketGroup *first_pkt = packet_list.first_pkt;
    packet_list.first_pkt = packet_list.first_pkt->next;
    return first_pkt->packet;
}

// 判断队列是否为空
int is_empty() {
    return packet_list.first_pkt==NULL ? 1 : 0;
}

// 回调函数，在获取音频数据后调用
void fill_audio(void *para, uint8_t *stream, int len) {
    SDL_memset(stream, 0, len); // 将缓冲区清零
    if (audio_len == 0) {
        return;
    }
    len = (len > audio_len ? audio_len : len);
    // 将音频数据混合到缓冲区
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
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
    }
    return 0;
}

// 初始化图像转换器的实例
int init_sws_context(void) {
    target_width = 480;
    target_height = target_width*video_decode_ctx->height/video_decode_ctx->width;
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    swsContext = sws_getContext(
            video_decode_ctx->width, video_decode_ctx->height, AV_PIX_FMT_YUV420P, 
            target_width, target_height, target_format, 
            SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (swsContext == NULL) {
        av_log(NULL, AV_LOG_ERROR, "swsContext is null\n");
        return -1;
    }
    sws_frame = av_frame_alloc(); // 分配一个RGB数据帧
    sws_frame->format = target_format; // 像素格式
    sws_frame->width = target_width; // 视频宽度
    sws_frame->height = target_height; // 视频高度
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(sws_frame->data, sws_frame->linesize, 
            target_width, target_height, target_format, 1);
//    // 分配缓冲区空间，用于存放转换后的图像数据
//    int buffer_size = av_image_get_buffer_size(target_format, target_width, target_height, 1);
//    unsigned char *out_buffer = (unsigned char*)av_malloc(
//                            (size_t)buffer_size * sizeof(unsigned char));
//    // 将数据帧与缓冲区关联
//    av_image_fill_arrays(sws_frame->data, sws_frame->linesize, out_buffer,
//                       target_format, target_width, target_height, 1);
    return 0;
}

// 分线程的任务处理
int thread_work(void *arg) {
    while (1) {
        if (is_close) { // 关闭窗口了
            break;
        }
        SDL_LockMutex(list_lock); // 对队列锁加锁
        if (is_empty()) {
            SDL_UnlockMutex(list_lock); // 对队列锁解锁
            SDL_Delay(5); // 延迟若干时间，单位毫秒
            continue;
        }
        AVPacket packet = pop_packet(); // 取出头部的视频包
        SDL_UnlockMutex(list_lock); // 对队列锁解锁

        if (packet.dts != AV_NOPTS_VALUE) { // 保存视频时钟
            video_time = av_q2d(src_video->time_base) * packet.dts;
        }
        AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
        // 发送压缩数据到解码器
        int ret = avcodec_send_packet(video_decode_ctx, &packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            continue;
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
            // 转换器开始处理图像数据，缩小图像尺寸
            sws_scale(swsContext, (const uint8_t* const*) frame->data, frame->linesize,
                0, frame->height, sws_frame->data, sws_frame->linesize);
            SDL_LockMutex(frame_lock); // 对帧锁加锁
            // 以下深度复制AVFrame（完整复制，不是简单引用）
            video_frame->format = sws_frame->format; // 像素格式（视频）或者采样格式（音频）
            video_frame->width = sws_frame->width; // 视频宽度
            video_frame->height = sws_frame->height; // 视频高度
//            video_frame->ch_layout = sws_frame->ch_layout; // 声道布局，音频需要
//            video_frame->nb_samples = sws_frame->nb_samples; // 采样数，音频需要
            av_frame_get_buffer(video_frame, 32); // 重新分配数据帧的缓冲区（储存视频或音频要用）
            av_frame_copy(video_frame, sws_frame); // 复制数据帧的缓冲区数据
            av_frame_copy_props(video_frame, sws_frame); // 复制数据帧的元数据
            play_video = 1; // 可以播放视频了
            SDL_UnlockMutex(frame_lock); // 对帧锁解锁
            if (has_audio) { // 存在音频流
                // 如果视频包太早被解码出来，就要等待同时刻的音频时钟
                while (video_time > audio_time) {
                    SDL_Delay(5); // 延迟若干时间，单位毫秒
                    if (is_close) {
                        break;
                    }
                }
            }
        }
        av_packet_unref(&packet); // 清除数据包
    }
    return 0;
}

// 准备SDL视频相关资源
int prepare_video(void) {
    int fps = av_q2d(src_video->r_frame_rate); // 帧率
    interval = round(1000 / fps); // 根据帧率计算每帧之间的播放间隔
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        av_log(NULL, AV_LOG_ERROR, "can not initialize SDL\n");
        return -1;
    }
    // 创建SDL窗口
    window = SDL_CreateWindow("Video Player",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        target_width, target_height, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        av_log(NULL, AV_LOG_ERROR, "can not create window\n");
        return -1;
    }
    // 创建SDL渲染器
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        av_log(NULL, AV_LOG_ERROR, "can not create renderer\n");
        return -1;
    }
    // 创建SDL纹理
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING, target_width, target_height);
    rect.x = 0; // 左上角的横坐标
    rect.y = 0; // 左上角的纵坐标
    rect.w = target_width; // 视频宽度
    rect.h = target_height; // 视频高度

    list_lock = SDL_CreateMutex(); // 创建互斥锁，用于调度队列
    frame_lock = SDL_CreateMutex(); // 创建互斥锁，用于调度视频帧
    // 创建SDL线程，指定任务处理函数，并返回线程编号
    sdl_thread = SDL_CreateThread(thread_work, "thread_work", NULL);
    if (!sdl_thread) {
        av_log(NULL, AV_LOG_ERROR, "sdl create thread occur error\n");
        return -1;
    }
    return 0;
}

// 准备SDL音频相关资源
int prepare_audio(void) {
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的声道布局
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; // 输出的采样格式
    int out_sample_rate = 44100; // 输出的采样率
    int out_nb_samples = audio_decode_ctx->frame_size; // 输出的采样数量
    int out_channels = out_ch_layout.nb_channels; // 输出的声道数量
    if (out_nb_samples == 0) {
        av_log(NULL, AV_LOG_ERROR, "unknown audio nb_samples\n");
        return -1;
    }
    int ret = swr_alloc_set_opts2(&swr_ctx, // 音频采样器的实例
                          &out_ch_layout, // 输出的声道布局
                          out_sample_fmt, // 输出的采样格式
                          out_sample_rate, // 输出的采样频率
                          &audio_decode_ctx->ch_layout, // 输入的声道布局
                          audio_decode_ctx->sample_fmt, // 输入的采样格式
                          audio_decode_ctx->sample_rate, // 输入的采样频率
                          0, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "swr_alloc_set_opts2 error %d\n", ret);
        return -1;
    }
    swr_init(swr_ctx); // 初始化音频采样器的实例
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "swr_init error %d\n", ret);
        return -1;
    }
    // 计算输出的缓冲区大小
    out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
    // 分配输出缓冲区的空间
    out_buff = (unsigned char *) av_malloc(MAX_AUDIO_FRAME_SIZE * out_channels);

    SDL_AudioSpec audio_spec; // 声明SDL音频参数
    audio_spec.freq = out_sample_rate; // 采样频率
    audio_spec.format = AUDIO_S16SYS; // 采样格式
    audio_spec.channels = out_channels; // 声道数量
    audio_spec.silence = 0; // 是否静音
    audio_spec.samples = out_nb_samples; // 采样数量
    audio_spec.callback = fill_audio; // 回调函数的名称
    audio_spec.userdata = NULL; // 回调函数的额外信息，如果没有额外信息就填NULL
    if (SDL_OpenAudio(&audio_spec, NULL) < 0) { // 打开扬声器
        av_log(NULL, AV_LOG_ERROR, "open audio occur error\n");
        return -1;
    }
    SDL_PauseAudio(0); // 播放/暂停音频。参数为0表示播放，为1表示暂停
    return 0;
}

// 释放资源
void release(void) {
    if (video_index >= 0) {
        sws_freeContext(swsContext); // 释放图像转换器的实例
        av_frame_free(&video_frame); // 释放数据帧资源
        int finish_status; // 线程的结束标志
        SDL_WaitThread(sdl_thread, &finish_status); // 等待线程结束，结束标志在status字段返回
        SDL_DestroyMutex(list_lock); // 销毁队列锁
        SDL_DestroyMutex(frame_lock); // 销毁帧锁
        av_log(NULL, AV_LOG_INFO, "sdl_thread finish_status=%d\n", finish_status);
        avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
        avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
        SDL_DestroyTexture(texture); // 销毁SDL纹理
        SDL_DestroyRenderer(renderer); // 销毁SDL渲染器
        SDL_DestroyWindow(window); // 销毁SDL窗口
    }
    if (audio_index >= 0) {
        avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
        avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
        swr_free(&swr_ctx); // 释放音频采样器的实例
        SDL_CloseAudio(); // 关闭扬声器
    }
    SDL_Quit(); // 退出SDL
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
}

int main(int argc, char **argv) {
    const char *src_name = "../fuzhou.mp4";
    if (argc > 1) {
        src_name = argv[1];
    }
    if (open_input_file(src_name) < 0) { // 打开输入文件
        return -1;
    }
    if (video_index >= 0) {
        if (init_sws_context() < 0) { // 初始化图像转换器的实例
            return -1;
        }
        if (prepare_video() < 0) { // 准备SDL视频相关资源
            return -1;
        }
    }
    if (audio_index >= 0) {
        if (prepare_audio() < 0) { // 准备SDL音频相关资源
            return -1;
        }
    }
    
    int ret;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *audio_frame = av_frame_alloc(); // 分配一个数据帧
    video_frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要解码
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0) {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(audio_decode_ctx, audio_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                } else if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "decode frame occur error %d.\n", ret);
                    continue;
                }
                while (audio_len > 0) { // 如果还没播放完，就等待1ms
                    SDL_Delay(1); // 延迟若干时间，单位毫秒
                }
                // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
                swr_convert(swr_ctx, // 音频采样器的实例
                    &out_buff, MAX_AUDIO_FRAME_SIZE, // 输出的数据内容和数据大小
                    (const uint8_t **) audio_frame->data, audio_frame->nb_samples); // 输入的数据内容和数据大小
                audio_pos = (unsigned char *) out_buff; // 把音频数据同步到缓冲区位置
                audio_len = out_buffer_size; // 缓冲区大小
                has_audio = 1; // 找到了音频流
                if (packet->pts != AV_NOPTS_VALUE) { // 保存音频时钟
                    audio_time = av_q2d(src_audio->time_base) * packet->pts;
                }
            } else {
                av_log(NULL, AV_LOG_ERROR, "send packet occur error %d.\n", ret);
            }
        } else if (packet->stream_index == video_index) { // 视频包需要解码
            SDL_LockMutex(list_lock); // 对队列锁加锁
            push_packet(*packet); // 把视频包加入队列
            SDL_UnlockMutex(list_lock); // 对队列锁解锁
            if (!has_audio) { // 不存在音频流
                SDL_Delay(interval); // 延迟若干时间，单位毫秒
            }
        }
        if (play_video) { // 允许播放视频
            SDL_LockMutex(frame_lock); // 对帧锁加锁
            play_video = 0;
            // 刷新YUV纹理
            SDL_UpdateYUVTexture(texture, NULL,
                video_frame->data[0], video_frame->linesize[0],
                video_frame->data[1], video_frame->linesize[1],
                video_frame->data[2], video_frame->linesize[2]);
            //SDL_RenderClear(renderer); // 清空渲染器
            SDL_RenderCopy(renderer, texture, NULL, &rect); // 将纹理复制到渲染器
            SDL_RenderPresent(renderer); // 渲染器开始渲染
//            av_log(NULL, AV_LOG_INFO, "render a video frame %lf %lf\n", video_time, audio_time);
            SDL_UnlockMutex(frame_lock); // 对帧锁解锁
            SDL_PollEvent(&event); // 轮询SDL事件
            switch (event.type) {
                case SDL_QUIT: // 如果命令关闭窗口（单击了窗口右上角的叉号）
                    goto __QUIT; // 这里用goto不用break
                default:
                    break;
            }
        }
        //av_packet_unref(packet); // 清除数据包（注意这里不能清除，因为从队列取出后已经清除）
    }
    av_log(NULL, AV_LOG_INFO, "Success play video file with audio stream.\n");
    
__QUIT:
    av_log(NULL, AV_LOG_INFO, "Close window.\n");
    is_close = 1;
    av_frame_free(&audio_frame); // 释放数据帧资源
    release(); // 释放资源
//    av_packet_free(&packet); // 释放数据包资源（不能重复调用av_packet_unref函数）
    av_log(NULL, AV_LOG_INFO, "Quit SDL.\n");
    return 0;
}
