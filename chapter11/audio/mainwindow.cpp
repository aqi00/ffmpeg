#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QBuffer>
#include <QtMultimedia/QAudioSink>
#include <QMediaDevices>
#include <QAudioDecoder>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 创建一个垂直布局
    QVBoxLayout *vBox = new QVBoxLayout(this->centralWidget());
    vBox->setAlignment(Qt::AlignTop); // 设置顶部对齐
    QPushButton *btn_choose = new QPushButton(); // 创建按钮控件
    btn_choose->setText("打开音频文件");
    vBox->addWidget(btn_choose); // 给布局添加按钮控件
    QLabel *label = new QLabel(); // 添加标签控件
    label->setText("这是一个标签"); // 设置标签文本
    label->setMaximumHeight(50); // 设置最大高度
    vBox->addWidget(label); // 给布局添加标签控件
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(btn_choose, &QPushButton::clicked, [=]() {
        // 对话框的输入参数依次为：上级窗口，对话框标题，默认目录，文件过滤器
        QString path = QFileDialog::getOpenFileName(this, "打开音频", "../file",
                "Audio files(*.mp3 *.aac *.m4a);;PCM files(*.pcm)");
        sprintf(m_audio_path, "%s", path.toStdString().c_str());
        qInfo() << "文件路径：" << m_audio_path << '\n';
        char strBuffer[256] = {0};
        sprintf(strBuffer, "音频文件路径是：%s", m_audio_path);
        label->setText(strBuffer); // 设置标签文本
    });

    btn_sdl = new QPushButton(); // 创建按钮控件
    btn_sdl->setText("sdl开始播放");
    vBox->addWidget(btn_sdl); // 给布局添加按钮控件
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(btn_sdl, &QPushButton::clicked, [=]() {
        is_stop = !is_stop;
        btn_sdl->setText(is_stop?"sdl开始播放":"sdl停止播放");
        if (is_stop) {
            stopAudio(); // 停止播放
        } else {
            playAudio(Play_SDL); // 开始播放
        }
    });

    btn_pause = new QPushButton(); // 创建按钮控件
    btn_pause->setText("暂停播放");
    btn_pause->setEnabled(false); // 禁用按钮
    vBox->addWidget(btn_pause); // 给布局添加按钮控件
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(btn_pause, &QPushButton::clicked, [=]() {
        pauseAudio(); // 暂停播放/恢复播放
    });

    btn_sink = new QPushButton(); // 创建按钮控件
    btn_sink->setText("sink开始播放");
    vBox->addWidget(btn_sink); // 给布局添加按钮控件
    // 注册按钮控件的单击事件。输入参数依次为：按钮，事件类型，回调方法
    connect(btn_sink, &QPushButton::clicked, [=]() {
        is_stop = !is_stop;
        btn_sink->setText(is_stop?"sink开始播放":"sink停止播放");
        if (is_stop) {
            stopAudio(); // 停止播放
        } else {
            playAudio(Play_SINK); // 开始播放
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 开始播放
void MainWindow::playAudio(PlayType play_type) {
    if (strlen(m_audio_path) <= 0) {
        QMessageBox::critical(this, "出错啦", "请先选择音频文件");
        return;
    }
    if (m_play_type != Unknown) {
        QMessageBox::critical(this, "出错啦", "请先停止另一种播放方式");
        return;
    }
    m_play_type = play_type;
    btn_pause->setEnabled(true); // 启用按钮
    btn_pause->setText(is_pause?"恢复播放":"暂停播放");
    if (QFileInfo(m_audio_path).suffix().toLower() == "pcm") {
        playPcmFile(); // 播放PCM文件
    } else if (m_play_type == Play_SDL) {
        sdl_player = new SdlPlayer(m_audio_path);
        sdl_player->start(); // 开始sdl方式播放
    } else if (m_play_type == Play_SINK) {
        sink_player = new SinkPlayer();
        sink_player->setFileName(m_audio_path);
        sink_player->start(); // 开始sink方式播放
    }
}

// 停止播放
void MainWindow::stopAudio() {
    if (QFileInfo(m_audio_path).suffix().toLower() == "pcm") {
        audio->stop(); // pcm停止播放
    } else if (m_play_type == Play_SDL) {
        sdl_player->stop(); // sdl方式停止播放
    } else if (m_play_type == Play_SINK) {
        sink_player->stop(); // sink方式停止播放
    }
    m_play_type = Unknown;
    btn_pause->setEnabled(false); // 禁用按钮
    is_pause = false;
}

// 暂停播放/恢复播放
void MainWindow::pauseAudio() {
    if (m_play_type == Unknown) {
        QMessageBox::critical(this, "出错啦", "请先播放音频文件");
        return;
    }
    is_pause = !is_pause;
    btn_pause->setText(is_pause?"恢复播放":"暂停播放");
    if (QFileInfo(m_audio_path).suffix().toLower() == "pcm") {
        if (is_pause) {
            audio->suspend(); // pcm文件暂停播放
        } else {
            audio->resume(); // pcm文件恢复播放
        }
    } else if (m_play_type == Play_SDL) {
        if (is_pause) {
            sdl_player->pause(); // sdl方式暂停播放
        } else {
            sdl_player->resume(); // sdl方式恢复播放
        }
    } else if (m_play_type == Play_SINK) {
        if (is_pause) {
            sink_player->pause(); // sink方式暂停播放
        } else {
            sink_player->resume(); // sink方式恢复播放
        }
    }
}

// 播放PCM文件
void MainWindow::playPcmFile() {
    QAudioFormat format;
    format.setSampleRate(44100); // 设置采样频率
    format.setChannelCount(2); // 设置声道数量
    format.setSampleFormat(QAudioFormat::Float); // 设置采样格式
    QAudioDevice device(QMediaDevices::defaultAudioOutput());
    if (!device.isFormatSupported(format)) { // 不支持该格式
        qWarning() << "Raw audio format not supported, cannot play audio.";
        return;
    } else {
        qInfo() << "Raw audio format is supported.";
    }
    audio = new QAudioSink(device, format); // 创建一个音频槽
    //connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
    file.setFileName(m_audio_path); // 设置文件路径
    file.open(QIODevice::ReadOnly); // 以只读方式打开文件
    qInfo() << "file.open " << m_audio_path << '\n';
    audio->start(&file); // 开始播放PCM文件
}

void MainWindow::handleStateChanged(QAudio::State newState)
{
    qInfo() << "newState " << newState << '\n';
    switch (newState) {
    case QAudio::IdleState:
        qInfo() << "IdleState" << '\n';
        audio->stop();
        file.close();
        delete audio;
        break;
    case QAudio::StoppedState:
        qInfo() << "StoppedState" << '\n';
        if (audio->error() != QAudio::NoError) {
            // Error handling
        }
        break;
    default:
        qInfo() << "default" << '\n';
        // ... other cases as appropriate
        break;
    }
}
