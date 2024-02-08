#ifndef SINKPLAYER_H
#define SINKPLAYER_H

#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioDecoder>

// 由于FFmpeg库使用C语言实现，因此告诉编译器要遵循C语言的编译规则
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
};

class SinkPlayer : public QObject
{
    Q_OBJECT
public:
    explicit SinkPlayer(QObject *parent = nullptr);
    ~SinkPlayer();
    void setFileName(const char *file_path);

    void start(); // 开始播放
    void stop(); // 停止播放
    void pause(); // 暂停播放
    void resume(); // 恢复播放
private:
    const char *m_audio_path; // 音频文件的路径
    bool is_stop = false; // 是否停止播放
    bool is_pause = false; // 是否暂停播放

    QAudioSink *sink = NULL; // 音频槽
    QIODevice *io; // 输入输出设备

    int playAudio(); // 播放音频
    void sleep(long millisecond); // 睡眠若干毫秒
};

#endif // SINKPLAYER_H
