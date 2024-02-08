#include "videoplayer.h"
#include <QDebug>
#include <thread>

#define MAX_AUDIO_FRAME_SIZE 80960 // 一帧音频最大长度（样本数），该值不能太小
int audio_len = 0; // 一帧PCM音频的数据长度
unsigned char *audio_pos = NULL; // 当前读取的位置

AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例
AVCodecContext *audio_decode_ctx = NULL; // 音频解码器的实例
int video_index = -1; // 视频流的索引
int audio_index = -1; // 音频流的索引
AVStream *src_video = NULL; // 源文件的视频流
AVStream *src_audio = NULL; // 源文件的音频流

int interval; // 视频帧之间的播放间隔
int can_play_video = 0; // 是否正在播放视频
int is_close = 0; // 是否关闭窗口
int has_audio = 0; // 是否拥有音频流
double audio_time = 0; // 音频时钟，当前音频包对应的时间值
SDL_mutex *audio_list_lock = NULL; // 声明一个音频包队列锁，防止线程间同时操作包队列
SDL_Thread *audio_thread = NULL; // 声明一个音频处理线程
std::list<AVPacket> packet_audio_list; // 存放音频包的队列

double video_time = 0; // 视频时钟，当前视频包对应的时间值
SDL_mutex *video_list_lock = NULL; // 声明一个视频包的队列锁，防止线程间同时操作包队列
SDL_mutex *frame_lock = NULL; // 声明一个帧锁，防止线程间同时操作视频帧
SDL_Thread *video_thread = NULL; // 声明一个视频处理线程
std::list<AVPacket> packet_video_list; // 存放视频包的队列

SwrContext *swr_ctx = NULL; // 音频采样器的实例
int out_buffer_size = 0; // 音频缓冲区的大小
unsigned char *out_buff = NULL; // 音频缓冲区的位置

enum AVSampleFormat out_sample_fmt; // 输出的采样格式
int out_sample_rate; // 输出的采样率
int out_nb_samples; // 输出的采样数量
int out_channels; // 输出的声道数量

SwsContext *m_yuv_sws = NULL;
AVFrame *m_yuv_frame = NULL;
uint8_t *m_yuv_buffer = NULL;
uint8_t *mYuv420Buffer = NULL;

bool is_stop = false; // 是否停止解码
bool is_pause = false; // 是否暂停解码
int target_height; // 实际的目标高度
int target_width; // 实际的目标宽度
bool is_over = false; // 是否全部结束

// 构造方法
VideoPlayer::VideoPlayer(const char *video_path, VideoCallBack *callback)
{
    qInfo() << "VideoPlayer video_path=" << video_path << '\n';
    m_video_path = video_path;
    m_callback = callback;
}

// 析构方法
VideoPlayer::~VideoPlayer()
{
    clear();
    release(); // 释放资源
}

// 回调函数，在获取音频数据后调用
void VideoPlayer::fill_audio(void *para, uint8_t *stream, int len) {
    SDL_memset(stream, 0, len); // 将缓冲区清零
    if (audio_len == 0) {
        return;
    }
    while (len > 0) { // 每次都要凑足len个字节才能退出循环
        int fill_len = (len > audio_len ? audio_len : len);
        // 将音频数据混合到缓冲区
        SDL_MixAudio(stream, audio_pos, fill_len, SDL_MIX_MAXVOLUME);
        audio_pos += fill_len;
        audio_len -= fill_len;
        len -= fill_len;
        stream += fill_len;
        if (audio_len == 0) { // 这里要延迟一会儿，避免一直占据IO资源
            SDL_Delay(1);
        }
        if (is_stop) {
            break;
        }
    }
}

// 打开输入文件
int VideoPlayer::open_input_file(const char *src_name) {
    in_fmt_ctx = avformat_alloc_context(); // 输入文件的封装器实例
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, NULL, NULL);
    if (ret < 0) {
        qCritical() << "Can't open file " << src_name << '\n';
        return -1;
    }
    qInfo() << "Success open input_file " << src_name << '\n';
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        qCritical() << "Can't find stream information" << '\n';
        return -1;
    }
    video_decode_ctx = NULL; // 视频解码器的实例
    src_video = NULL;
    // 找到视频流的索引
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            qCritical() << "video_codec not found" << '\n';
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            qCritical() << "video_decode_ctx is null" << '\n';
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            qCritical() << "Can't open video_decode_ctx" << '\n';
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
            qCritical() << "audio_codec not found" << '\n';
            return -1;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            qCritical() << "audio_decode_ctx is null" << '\n';
            return -1;
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        ret = avcodec_open2(audio_decode_ctx, audio_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            qCritical() << "Can't open audio_decode_ctx" << '\n';
            return -1;
        }
    }
    return 0;
}

// 初始化图像转换器的实例
int VideoPlayer::init_sws_context() {
    int origin_width = video_decode_ctx->width;
    int origin_height = video_decode_ctx->height;
    AVRational aspect_ratio = src_video->codecpar->sample_aspect_ratio;
    AVRational display_aspect_ratio;
    av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
              origin_width  * aspect_ratio.num,
              origin_height * aspect_ratio.den,
              1024 * 1024);
    char desc[1024];
    sprintf(desc, "SAR %d:%d, DAR %d:%d\n",
           aspect_ratio.num, aspect_ratio.den,
           display_aspect_ratio.num, display_aspect_ratio.den);
    qInfo() << desc << '\n';
    target_height = origin_height;
    target_width = target_height*origin_width/origin_height;
    // 根据实际的显示宽高比调整画面大小
    if (aspect_ratio.num!=0 && aspect_ratio.den!=0 && aspect_ratio.num!=aspect_ratio.den) {
        target_width = target_height*display_aspect_ratio.num/display_aspect_ratio.den;
    }
//    int width = video_decode_ctx->width;
//    int height = video_decode_ctx->height;
    enum AVPixelFormat yuv_format = AV_PIX_FMT_YUV420P; // OpenGL支持YUV空间
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    m_yuv_sws = sws_getContext(origin_width, origin_height, video_decode_ctx->pix_fmt,
                               target_width, target_height, yuv_format, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (m_yuv_sws == NULL) {
        qCritical() << "yuv swsContext is null" << '\n';
        return -1;
    }
    m_yuv_frame = av_frame_alloc(); // 分配一个YUV数据帧
    // 分配缓冲区空间，用于存放转换后的图像数据
    int yuv_size = av_image_get_buffer_size(yuv_format, target_width, target_height, 1);
    m_yuv_buffer = (uint8_t *)av_malloc((size_t)yuv_size * sizeof(uint8_t));
    // 将数据帧与缓冲区关联
    av_image_fill_arrays(m_yuv_frame->data, m_yuv_frame->linesize, m_yuv_buffer,
                         yuv_format, target_width, target_height, 1);
    mYuv420Buffer = (uint8_t*)malloc(target_width * target_height * 3 / 2);
    return 0;
}

// 音频分线程的任务处理
int VideoPlayer::thread_work_audio(void *arg) {
    qInfo() << "begin thread_work_audio" << '\n';
    int swr_size = 0;
    while (1) {
        if (is_stop || (is_close && packet_audio_list.empty())) { // 停止播放或者关闭窗口
            break;
        }
        SDL_LockMutex(audio_list_lock); // 对音频队列锁加锁
        if (packet_audio_list.empty()) {
            SDL_UnlockMutex(audio_list_lock); // 对音频队列锁解锁
            SDL_Delay(5); // 延迟若干时间，单位毫秒
            continue;
        }
        AVPacket packet = packet_audio_list.front(); // 取出头部的音频包
        packet_audio_list.pop_front(); // 队列头部出列
        SDL_UnlockMutex(audio_list_lock); // 对音频队列锁解锁
        AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
        // 发送压缩数据到解码器
        int ret = avcodec_send_packet(audio_decode_ctx, &packet);
        if (ret < 0) {
            qCritical() << "send packet occur error " << ret << '\n';
            continue;
        }
        while (1) {
            // 从解码器实例获取还原后的数据帧
            ret = avcodec_receive_frame(audio_decode_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                qCritical() << "decode frame occur error " << ret << '\n';
                break;
            }
            //qInfo() << "audio pts " <<  packet.pts;
            while (audio_len > 0) { // 如果还没播放完，就等待1ms
                SDL_Delay(1); // 延迟若干时间，单位毫秒
            }
            // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
            swr_size = swr_convert(swr_ctx, // 音频采样器的实例
                        &out_buff, MAX_AUDIO_FRAME_SIZE, // 输出的数据内容和数据大小
                        (const uint8_t **) frame->data, frame->nb_samples); // 输入的数据内容和数据大小
            audio_pos = (unsigned char *) out_buff; // 把音频数据同步到缓冲区位置
            // 这里要计算实际的采样位数
            audio_len = swr_size * out_channels * av_get_bytes_per_sample(out_sample_fmt);
            //qInfo() << "audio_len=" << audio_len << '\n';
            has_audio = 1; // 找到了音频流
            if (packet.pts != AV_NOPTS_VALUE) { // 保存音频时钟
                audio_time = av_q2d(src_audio->time_base) * packet.pts;
            }
            qInfo() << "audio pts " << frame->pts;
        }
        av_packet_unref(&packet); // 清除数据包
        while (is_pause) { // 如果暂停播放，就持续休眠；直到恢复播放才继续解码
            SDL_Delay(20); // 休眠20毫秒
            if (is_stop) { // 暂停期间如果停止播放，就结束暂停
                return -1;
            }
        }
    }
    return 0;
}

// 视频分线程的任务处理
int VideoPlayer::thread_work_video(void *arg) {
    qInfo() << "begin thread_work_video" << '\n';
    while (1) {
        if (is_stop || (is_close && packet_video_list.empty())) { // 停止播放或者关闭窗口
            break;
        }
        SDL_LockMutex(video_list_lock); // 对视频队列锁加锁
        if (packet_video_list.empty()) {
            SDL_UnlockMutex(video_list_lock); // 对视频队列锁解锁
            SDL_Delay(5); // 延迟若干时间，单位毫秒
            continue;
        }
        AVPacket packet = packet_video_list.front(); // 取出头部的视频包
        packet_video_list.pop_front(); // 队列头部出列
        SDL_UnlockMutex(video_list_lock); // 对视频队列锁解锁

        if (packet.dts != AV_NOPTS_VALUE) { // 保存视频时钟
            video_time = av_q2d(src_video->time_base) * packet.dts;
        }
        AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
        // 发送压缩数据到解码器
        int ret = avcodec_send_packet(video_decode_ctx, &packet);
        if (ret < 0) {
            qCritical() << "send packet occur error " << ret << '\n';
            return -1;
        }
        if (video_decode_ctx == NULL) {
            qInfo() << "video_decode_ctx is null" << '\n';
            return -1;
        }
        while (1) {
            // 从解码器实例获取还原后的数据帧
            ret = avcodec_receive_frame(video_decode_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                qCritical() << "decode frame occur error " << ret << '\n';
                return -1;
            }
            SDL_LockMutex(frame_lock); // 对帧锁加锁
            // 转换器开始处理图像数据，把视频帧转为YUV图像
            sws_scale(m_yuv_sws, (uint8_t const * const *) frame->data, frame->linesize,
                      0, target_height, m_yuv_frame->data, m_yuv_frame->linesize);
            int Ysize = target_width * target_height;
            memcpy(mYuv420Buffer, m_yuv_buffer, Ysize * 3 / 2);
            can_play_video = 1; // 可以播放视频了
            qInfo() << "video pts " << frame->pts;
            SDL_UnlockMutex(frame_lock); // 对帧锁解锁
            if (has_audio) { // 存在音频流
                // 如果视频包太早被解码出来，就要等待同时刻的音频时钟
                while (video_time>audio_time && !is_stop) {
                    SDL_Delay(5); // 延迟若干时间，单位毫秒
                    if (is_stop) {
                        break;
                    }
                }
            }

        }
        av_packet_unref(&packet); // 清除数据包
    }
    return 0;
}

// 准备视频相关资源
int VideoPlayer::prepare_video(void) {
    int fps = av_q2d(src_video->r_frame_rate); // 帧率
    interval = round(1000 / fps); // 根据帧率计算每帧之间的播放间隔

    video_list_lock = SDL_CreateMutex(); // 创建互斥锁，用于调度队列
    frame_lock = SDL_CreateMutex(); // 创建互斥锁，用于调度视频帧
    // 创建SDL线程，指定任务处理函数，并返回线程编号
    video_thread = SDL_CreateThread(thread_work_video, "thread_work_video", NULL);
    if (!video_thread) {
        qCritical() << "sdl create video thread occur error" << '\n';
        return -1;
    }
    return 0;
}

// 准备音频相关资源
int VideoPlayer::prepare_audio() {
    AVChannelLayout out_ch_layout = audio_decode_ctx->ch_layout; // 输出的声道布局
    out_sample_fmt = AV_SAMPLE_FMT_S16; // 输出的采样格式
    out_sample_rate = audio_decode_ctx->sample_rate; // 输出的采样率
    out_nb_samples = audio_decode_ctx->frame_size; // 输出的采样数量
    out_channels = out_ch_layout.nb_channels; // 输出的声道数量
    if (out_nb_samples <= 0) {
        out_nb_samples = 512;
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
        qCritical() << "swr_alloc_set_opts2 error " << ret << '\n';
        return -1;
    }
    ret = swr_init(swr_ctx); // 初始化音频采样器的实例
    if (ret < 0) {
        qCritical() << "swr_init error " << ret << '\n';
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
        qCritical() << "open audio occur error" << '\n';
        return -1;
    }
    SDL_PauseAudio(0); // 播放/暂停音频。参数为0表示播放，为1表示暂停

    audio_list_lock = SDL_CreateMutex(); // 创建互斥锁，用于调度队列
    // 创建SDL线程，指定任务处理函数，并返回线程编号
    audio_thread = SDL_CreateThread(thread_work_audio, "thread_work_audio", NULL);
    if (!audio_thread) {
        qCritical() << "sdl create audio thread occur error" << '\n';
        return -1;
    }
    return 0;
}

// 播放视频画面
int VideoPlayer::play_video_frame() {
    if (can_play_video) { // 允许播放视频
        SDL_LockMutex(frame_lock); // 对帧锁加锁
        can_play_video = 0;
        // 图像数据已经准备好，可以显示图像了
        displayImage(target_width, target_height);
        SDL_UnlockMutex(frame_lock); // 对帧锁解锁
    }
    while (is_pause) { // 如果暂停播放，就持续休眠；直到恢复播放才继续解码
        SDL_Delay(20); // 休眠20毫秒
        if (is_stop) { // 暂停期间如果停止播放，就结束暂停
            return -1;
        }
    }
    if (is_stop) {
        return -1;
    }
    return 0;
}

// 释放资源
void VideoPlayer::release() {
    if (audio_index >= 0) {
        qInfo() << "begin release audio resource" << '\n';
        int audio_status; // 线程的结束标志
        SDL_WaitThread(audio_thread, &audio_status); // 等待线程结束，结束标志在status字段返回
        SDL_DestroyMutex(audio_list_lock); // 销毁音频队列锁
        qInfo() << "audio_thread audio_status=" << audio_status << '\n';
        avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
        avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
        swr_free(&swr_ctx); // 释放音频采样器的实例
        SDL_CloseAudio(); // 关闭扬声器
        qInfo() << "end release audio resource" << '\n';
    }
    if (video_index >= 0) {
        qInfo() << "begin release video resource" << '\n';
        sws_freeContext(m_yuv_sws); // 释放图像转换器的实例
        int video_status; // 线程的结束标志
        SDL_WaitThread(video_thread, &video_status); // 等待线程结束，结束标志在status字段返回
        SDL_DestroyMutex(video_list_lock); // 销毁视频队列锁
        SDL_DestroyMutex(frame_lock); // 销毁帧锁
        qInfo() << "video_thread video_status=" << video_status << '\n';
        avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
        avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
        qInfo() << "end release video resource" << '\n';
    }
    SDL_Quit(); // 退出SDL
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    clear();
    m_callback->onStopPlay(); // 通知界面修改按钮状态
}

// 开始播放
const char *VideoPlayer::start()
{
    reset(); // 重置
    if (open_input_file(m_video_path) < 0) { // 打开输入文件
        release();
        return "打开文件失败！";
    }
    qInfo() << "end open_input_file " << m_video_path << '\n';
    if (video_index >= 0) { // 存在视频流
        if (init_sws_context() < 0) { // 初始化图像转换器的实例
            release();
            return "初始化转换器失败！";
        }
        if (prepare_video() < 0) { // 准备视频相关资源
            release();
            return "准备视频资源失败！";
        }
    }
    if (audio_index >= 0) { // 存在音频流
        if (prepare_audio() < 0) { // 准备音频相关资源
            clear();
            release();
            return "准备音频资源失败！";
        }
        qInfo() << "end prepare_audio " << m_video_path << '\n';
    }
    // 开启分线程播放视频。detach表示分离该线程
    std::thread([this](){
        int ret = playVideo(); // 播放视频
        qInfo() << "play result: " << ret << '\n';
    }).detach();
    return NULL;
}

// 停止播放
void VideoPlayer::stop()
{
    clear();
    do {
        SDL_Delay(20);
    } while (!is_over);
}

// 清除
void VideoPlayer::clear()
{
    is_pause = false;
    is_stop = true;
    is_close = 1;
}

// 暂停播放
void VideoPlayer::pause()
{
    is_pause = true;
}

// 恢复播放
void VideoPlayer::resume()
{
    is_pause = false;
}

// 重置
void VideoPlayer::reset()
{
    audio_time = 0;
    video_time = 0;
    video_index = -1;
    audio_index = -1;
    packet_audio_list.clear();
    packet_video_list.clear();
    is_stop = false;
    is_pause = false;
    is_close = 0;
    is_over = false;
    can_play_video = 0;
    has_audio = 0;
    m_yuv_buffer = NULL;
    mYuv420Buffer = NULL;
    out_buffer_size = 0;
    out_buff = NULL;
    audio_len = 0;
    audio_pos = NULL;
}

// 播放视频
int VideoPlayer::playVideo()
{
    qInfo() << "playVideo " << m_video_path << '\n';

    int ret;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要解码
            //qInfo() << "audio pts " << packet->pts;
            SDL_LockMutex(audio_list_lock); // 对音频队列锁加锁
            packet_audio_list.push_back(*packet); // 把音频包加入队列
            SDL_UnlockMutex(audio_list_lock); // 对音频队列锁解锁
        } else if (packet->stream_index == video_index) { // 视频包需要解码
            //qInfo() << "video pts " << packet->pts;
            SDL_LockMutex(video_list_lock); // 对视频队列锁加锁
            packet_video_list.push_back(*packet); // 把视频包加入队列
            SDL_UnlockMutex(video_list_lock); // 对视频队列锁解锁
            if (!has_audio) { // 不存在音频流
                SDL_Delay(interval); // 延迟若干时间，单位毫秒
            }
        }
        if (play_video_frame() == -1) { // 播放视频画面
            break;
        }
        if (!packet_audio_list.empty() && !packet_video_list.empty()) {
            SDL_Delay(15); // 延迟若干时间，单位毫秒
        }
        //av_packet_unref(packet); // 清除数据包（注意这里不能清除，因为从队列取出后已经清除）
    }
    while (!packet_video_list.empty()) { // 播放剩余的视频画面
        if (play_video_frame() == -1) {
            break;
        }
        SDL_Delay(5); // 延迟若干时间，单位毫秒
    }
    qInfo() << "Success play video file with audio stream" << '\n';

    is_close = 1;
    release(); // 释放资源
    qInfo() << "Quit Play" << '\n';
    is_over = true;
    return 0;
}

// 显示图像
void VideoPlayer::displayImage(int width, int height) {
    VideoFramePtr videoFrame = std::make_shared<VideoFrame>();
    VideoFrame *ptr = videoFrame.get();
    ptr->initBuffer(width, height); // 初始化图像缓存
    ptr->setYUVbuf(mYuv420Buffer); // 设置YUV缓存数据
    // 通知回调接口展示该帧视频的图像
    m_callback->onDisplayVideo(videoFrame);
}
