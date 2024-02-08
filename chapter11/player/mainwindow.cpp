#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    FunctionTransfer::init(QThread::currentThreadId());
    // 创建一个垂直布局
    QVBoxLayout *vBox = new QVBoxLayout(this->centralWidget());
    vBox->setAlignment(Qt::AlignTop); // 设置顶部对齐
    QPushButton *btn_choose = new QPushButton(); // 创建按钮控件
    btn_choose->setText("打开音视频文件");
    vBox->addWidget(btn_choose); // 给布局添加按钮控件
    QLabel *label = new QLabel(); // 创建标签控件
    label->setText("这是一个标签"); // 设置标签文本
    label->setMaximumHeight(50); // 设置最大高度
    vBox->addWidget(label); // 给布局添加标签控件
    // 创建一个水平布局
    QHBoxLayout *hBox = new QHBoxLayout(this->centralWidget());
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(btn_choose, &QPushButton::clicked, [=]() {
        // 对话框的输入参数依次为：上级窗口，对话框标题，默认目录，文件过滤器
        QString path = QFileDialog::getOpenFileName(this, "打开视频", "../file",
                "Video files(*.mp4 *.m4v *.mov *.3gp *.avc *.hevc *.ts *.flv *.asf *.wmv *.avi *.mkv *.mpg *.rm *.rmvb *.vob *.webm);;Audio files(*.mp3 *.aac *.m4a *.wav *.ra *.ogg *.amr *.wma *.opus)");
        if (!path.isEmpty()) {
            sprintf(m_video_path, "%s", path.toStdString().c_str());
            qInfo() << "文件路径：" << m_video_path << '\n';
            char strBuffer[256] = {0};
            sprintf(strBuffer, "音视频文件路径是：%s", m_video_path);
            label->setText(strBuffer); // 设置标签文本
            if (video_player != NULL && !is_stop) {
                playVideo();
                //SDL_Delay(200);
            }
            playVideo();
        }
    });
    btn_play = new QPushButton(); // 创建按钮控件
    btn_play->setText("开始播放");
    hBox->addWidget(btn_play); // 给布局添加按钮控件
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(btn_play, &QPushButton::clicked, [=]() {
        playVideo(); // 开始播放/停止播放
    });
    btn_pause = new QPushButton(); // 创建按钮控件
    btn_pause->setText("暂停播放");
    btn_pause->setEnabled(false); // 禁用按钮
    hBox->addWidget(btn_pause); // 给布局添加按钮控件
    vBox->addLayout(hBox); // 给布局添加另一个布局
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(btn_pause, &QPushButton::clicked, [=]() {
        pauseVideo(); // 暂停播放/恢复播放
    });
    opengl_view = new OpenglView(); // 创建OpenGL视图
    QSizePolicy policy;
    policy.setHorizontalPolicy(QSizePolicy::Expanding); // 水平方向允许伸展
    policy.setVerticalPolicy(QSizePolicy::Expanding); // 垂直方向允许伸展
    opengl_view->setSizePolicy(policy); // 设置控件的大小策略
    opengl_view->setMinimumWidth(480); // 设置最小宽度
    opengl_view->setMinimumHeight(270); // 设置最小高度
    vBox->addWidget(opengl_view); // 给布局添加OpenGL视图

    std::map<QString, QString> program_map;
    program_map["  在此选择直播网址"] = "";
    program_map["2018年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2018/04/17/20180417_162d3639356_r38_1200k.mp4";
    program_map["2019年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2019/04/15/20190415_16a1ef11c24_r38_1200k.mp4";
    program_map["2020年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2020/09/26/20200926_174c8f9e4b6_r38_1200k.mp4";
    program_map["2021年数字中国峰会迎宾曲"] = "http://flv4mp4.people.com.cn/videofile7/pvmsvideo/2021/3/20/FuJianWuZhou_d0cdcc84ccf1f8e561544d422e470a7f.mp4";
    program_map["2022年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2022/07/15/20220715_18201603713_r38_1200k.mp4";
    program_map["2023年数字中国峰会迎宾曲"] = "https://www.fujian.gov.cn/masvod/public/2023/04/25/20230425_187b71018de_r38_1200k.mp4";
    program_map["福建三明新闻综合频道"] = "https://stream.smntv.cn/smtv1/playlist.m3u8";
    program_map["福建三明公共频道"] = "https://stream.smntv.cn/smtv2/playlist.m3u8";
    program_map["江苏南京少儿频道"] = "https://live.nbs.cn/channels/njtv/sepd/m3u8:500k/live.m3u8";
    program_map["江苏南京娱乐频道"] = "https://live.nbs.cn/channels/njtv/ylpd/m3u8:500k/live.m3u8";
    program_map["江苏南京十八频道"] = "https://live.nbs.cn/channels/njtv/sbpd/m3u8:500k/live.m3u8";
    program_map["江苏南京信息频道"] = "https://live.nbs.cn/channels/njtv/xxpd/m3u8:500k/live.m3u8";
    program_map["广西南宁新闻综合频道"] = "https://hls.nntv.cn/nnlive/NNTV_NEWS_A.m3u8";
    program_map["广西南宁都市生活频道"] = "https://hls.nntv.cn/nnlive/NNTV_METRO_A.m3u8";
    program_map["广西南宁影视娱乐频道"] = "https://hls.nntv.cn/nnlive/NNTV_VOD_A.m3u8";
    program_map["广西南宁公共频道"] = "https://hls.nntv.cn/nnlive/NNTV_PUB_A.m3u8";
    program_map["广东河源综合频道"] = "https://tmpstream.hyrtv.cn/xwzh/sd/live.m3u8";
    program_map["广东河源公共频道"] = "https://tmpstream.hyrtv.cn/hygg/sd/live.m3u8";
    QComboBox *program_list = new QComboBox(); // 创建下拉框控件
    std::map<QString, QString>::iterator it;
    for (it = program_map.begin(); it != program_map.end(); it++)//begin和end均返回迭代器类型
    {
        program_list->addItem(it->first);
    }
    vBox->addWidget(program_list); // 给布局添加下拉框控件
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(program_list, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, [=](const QString &text) {
        if (!text.isEmpty() && !program_map.at(text).isEmpty()) {
            sprintf(m_video_path, "%s", program_map.at(text).toStdString().c_str());
            qInfo() << "直播路径：" << m_video_path << '\n';
            char strBuffer[256] = {0};
            sprintf(strBuffer, "音视频直播路径是：%s", m_video_path);
            label->setText(strBuffer); // 设置标签文本
            if (video_player != NULL && !is_stop) {
                playVideo();
                //SDL_Delay(200);
            }
            playVideo();
        }
    });
}

MainWindow::~MainWindow()
{
    if (video_player != NULL) {
        video_player->stop(); // 停止播放
    }
    delete opengl_view;
    delete ui;
}

// 开始播放/停止播放
void MainWindow::playVideo() {
    if (m_video_path == NULL || strlen(m_video_path) <= 0) {
        QMessageBox::critical(this, "出错啦", "请先选择视频文件");
        return;
    }
    is_stop = !is_stop;
    btn_play->setText(is_stop?"开始播放":"停止播放");
    if (is_stop) {
        video_player->stop(); // 停止播放
        btn_pause->setEnabled(false); // 禁用按钮
        is_pause = false;
    } else {
        btn_pause->setEnabled(true); // 启用按钮
        btn_pause->setText(is_pause?"恢复播放":"暂停播放");
        // 创建视频播放器
        video_player = new VideoPlayer(m_video_path, this);
        const char *result = video_player->start(); // 开始播放
        if (result != NULL) {
            QMessageBox::critical(this, "播放失败", result);
        }
    }
    qInfo() << "opengl_view->width=" << opengl_view->width() << ", opengl_view->height=" << opengl_view->height() << '\n';
}

// 暂停播放/恢复播放
void MainWindow::pauseVideo() {
    if (is_stop == true) {
        QMessageBox::critical(this, "出错啦", "请先播放视频文件");
        return;
    }
    is_pause = !is_pause;
    btn_pause->setText(is_pause?"恢复播放":"暂停播放");
    if (is_pause) {
        video_player->pause(); // 暂停播放
    } else {
        video_player->resume(); // 恢复播放
    }
}

// 显示视频画面，此函数不宜做耗时操作，否则会影响播放的流畅性。
void MainWindow::onDisplayVideo(VideoFramePtr videoFrame)
{
    qInfo() << "onDisplayVideo frame->width=" << videoFrame->width() << ", frame->height=" << videoFrame->height() << '\n';
    opengl_view->onFrameDecoded(videoFrame); // OpenGL视图展示画面
}

// 停止播放。通知界面修改按钮状态
void MainWindow::onStopPlay()
{
    is_stop = true;
    btn_play->setText("开始播放");
    is_pause = false;
    btn_pause->setText("暂停播放");
    btn_pause->setEnabled(false); // 启用按钮
}
