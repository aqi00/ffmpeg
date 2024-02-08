#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QLabel>
#include <QVBoxLayout>

// 由于FFmpeg库使用C语言实现，因此告诉编译器要遵循C语言的编译规则
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>
#include <libavfilter/version.h>
#include <libswresample/version.h>
#include <libswscale/version.h>
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "　libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\n　libavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\n　libavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\n　libavfilter : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFILTER_VERSION));
    strcat(strBuffer, "\n　libswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\n　libswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\n　avcodec_configure : \n");
    strcat(strBuffer, avcodec_configuration());
    strcat(strBuffer, "\n　avcodec_license : ");
    strcat(strBuffer, avcodec_license());
    // 创建一个垂直布局
    QVBoxLayout *vBox = new QVBoxLayout(this->centralWidget());
    vBox->setAlignment(Qt::AlignTop); // 设置布局的对齐方式
    QLabel *label = new QLabel(); // 创建标签控件
    label->setWordWrap(true); // 允许文字换行
    label->setText(strBuffer); // 设置标签文本
    vBox->addWidget(label); // 给布局添加标签控件
}

MainWindow::~MainWindow()
{
    delete ui;
}

