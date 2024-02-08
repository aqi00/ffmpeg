#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QLabel *label = new QLabel(this->centralWidget()); // 创建标签控件
    label->setText("Hello World"); // 设置标签文本
}

MainWindow::~MainWindow()
{
    delete ui;
}

