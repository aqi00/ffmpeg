#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>

#include "util/FunctionTransfer.h"
#include "util/VideoFrame.h"
#include "util/videocallback.h"
#include "videoview.h"
#include "openglview.h"
#include "videodecoder.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public VideoCallBack
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QPushButton *btn_play;
    QPushButton *btn_pause;
    VideoView *video_view;
    QPushButton *btn_play2;
    OpenglView *opengl_view;

    char m_video_path[128] = {NULL};
    bool is_stop = true;
    bool is_pause = false;
    int m_play_type = -1;

    VideoDecoder *video_decoder = NULL; // 视频解码器

    void playVideo(int play_type); // 开始播放视频/停止播放视频
    void pauseVideo(); // 暂停播放视频/恢复播放视频
protected:
    // 显示视频画面，此函数不宜做耗时操作，否则会影响播放的流畅性。
    void onDisplayVideo(VideoFramePtr videoFrame);
    // 停止播放。通知界面修改按钮状态
    void onStopPlay();
};
#endif // MAINWINDOW_H
