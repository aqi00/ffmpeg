#include "sinkplayer.h"
#include <QDebug>
#include <thread>

#define MAX_AUDIO_FRAME_SIZE 8096 // 一帧音频最大长度（样本数），该值不能太小
int out_sample_rate = 44100; // 输出的采样率
AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的声道布局
enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; // 输出的采样格式
enum QAudioFormat::SampleFormat q_sample_fmt = QAudioFormat::Int16;

// 构造方法
SinkPlayer::SinkPlayer(QObject *parent) : QObject{parent}
{
    QAudioFormat format;
    format.setSampleRate(out_sample_rate); // 设置采样频率
    format.setChannelCount(out_ch_layout.nb_channels); // 设置声道数量
    format.setSampleFormat(q_sample_fmt); // 设置采样格式
    qInfo("sampleRate: %d, channelCount: %d, sampleFormat: %d",
          format.sampleRate(), format.channelCount(), format.sampleFormat());
    QAudioDevice device(QMediaDevices::defaultAudioOutput());
    if (!device.isFormatSupported(format)) { // 不支持该格式
        qWarning() << "Raw audio format not supported by backend, cannot play audio->";
    } else {
        qInfo() << "Raw audio format is supported.";
    }
    sink = new QAudioSink(device, format);		//创建音频输出设备
}

// 析构方法
SinkPlayer::~SinkPlayer()
{
    sink->stop(); // 停止播放
}

void SinkPlayer::setFileName(const char *file_path)
{
    qInfo() << "SinkPlayer::setFileName" << file_path << '\n';
    m_audio_path = file_path;
}

// 开始播放
void SinkPlayer::start()
{
    is_stop = false;
    qInfo() << "play audio: " << m_audio_path << '\n';
    io = sink->start(); // 开始播放
    // 开启分线程播放音频。detach表示分离该线程
    std::thread([this](){
        int ret = playAudio(); // 播放音频
        qInfo() << "play result: " << ret << '\n';
    }).detach();
}

// 停止播放
void SinkPlayer::stop()
{
    is_stop = true;
    sink->stop(); // 停止播放
}

// 暂停播放
void SinkPlayer::pause()
{
    is_pause = true;
    sink->suspend(); // 暂停播放
}

// 恢复播放
void SinkPlayer::resume()
{
    is_pause = false;
    sink->resume(); // 恢复播放
}

// 播放音频
int SinkPlayer::playAudio()
{
    qInfo() << "playAudio " << m_audio_path << '\n';
    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, m_audio_path, NULL, NULL);
    if (ret < 0) {
        qCritical() << "Can't open file " << m_audio_path << '\n';
        return -1;
    }
    qInfo() << "Success open input_file " << m_audio_path << '\n';
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        qCritical() << "Can't find stream information" << '\n';
        return -1;
    }
    AVCodecContext *audio_decode_ctx = NULL; // 音频解码器的实例
    AVStream *src_audio = NULL;
    // 找到音频流的索引
    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
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
    } else {
        qCritical() << "Can't find audio stream" << '\n';
        return -1;
    }

    qInfo() << "begin swr_init" << '\n';
    int out_nb_samples = audio_decode_ctx->frame_size; // 输出的采样数量
    int out_channels = out_ch_layout.nb_channels; // 输出的声道数量
    SwrContext *swr_ctx = NULL; // 音频采样器的实例
    ret = swr_alloc_set_opts2(&swr_ctx, // 音频采样器的实例
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
    swr_init(swr_ctx); // 初始化音频采样器的实例
    if (ret < 0) {
        qCritical() << "swr_init error " << ret << '\n';
        return -1;
    }

    // 计算输出的缓冲区大小
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
    qInfo() << "out_buffer_size=" << out_buffer_size << '\n';
    // 分配输出缓冲区的空间
    unsigned char *out_buff = (unsigned char *) av_malloc(MAX_AUDIO_FRAME_SIZE * out_channels);
    qInfo() << "begin play audio" << '\n';

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要解码
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0) {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(audio_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                } else if (ret < 0) {
                    qCritical() << "decode frame occur error " << ret << '\n';
                    continue;
                }
                // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
                swr_convert(swr_ctx, // 音频采样器的实例
                            &out_buff, MAX_AUDIO_FRAME_SIZE, // 输出的数据内容和数据大小
                            (const uint8_t **) frame->data, frame->nb_samples); // 输入的数据内容和数据大小
                // 往扬声器写入音频数据
                io->write((const char*)(char*)out_buff, out_buffer_size);
                qInfo() << "swr_frame->nb_samples=" << frame->nb_samples;
                int delay = 1000 * frame->nb_samples / out_sample_rate;
                qInfo() << "delay=" << delay;
                sleep(delay); // 休眠若干时间，单位毫秒
            } else {
                qCritical() << "send packet occur error " << ret << '\n';
            }
            while (is_pause) { // 如果暂停播放，就持续休眠；直到恢复播放才继续解码
                sleep(20); // 休眠20毫秒
                if (is_stop) { // 暂停期间如果停止播放，就结束暂停
                    break;
                }
            }
            if (is_stop) { // 如果停止播放，就跳出循环结束解码
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    qInfo() << "Success play audio file" << '\n';

    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(audio_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    swr_free(&swr_ctx); // 释放音频采样器的实例
    qInfo() << "Quit Play" << '\n';
    return 0;
}

// 睡眠若干毫秒
void SinkPlayer::sleep(long millisecond) {
#ifdef _WIN32
    _sleep(millisecond);
#else
    usleep(millisecond * 1000);
#endif
}
