#include "openglview.h"
#include "./ui_openglview.h"

#include <QPainter>
#include <QDebug>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QFile>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QMouseEvent>

#include <QTimer>
#include <QDrag>
#include <QMimeData>

#include <QApplication>
#include <QScreen>
#include <QDateTime>

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4

OpenglView::OpenglView(QWidget *parent) :
    QOpenGLWidget(parent),
    ui(new Ui::OpenglView)
{
    ui->setupUi(this);
    connect(ui->pushButton_close, &QPushButton::clicked, this, &OpenglView::sig_CloseBtnClick);
    ui->pushButton_close->hide();
    ui->widget_erro->hide();
    ui->widget_name->hide();

    textureUniformY = 0;
    textureUniformU = 0;
    textureUniformV = 0;
    mHeight = 0;
    mWidth = 0;
    mPicIndexX = 0;
    mPicIndexY = 0;

    pVShader = NULL;
    pFShader = NULL;
    pShaderProgram = NULL;
    pTextureY = NULL;
    pTextureU = NULL;
    pTextureV = NULL;

    m_vertexVertices = new GLfloat[8];
    mVideoFrame.reset();
    setAcceptDrops(true);

    mIsPlaying = false;
    mPlayFailed = false;
    mCurrentVideoKeepAspectRatio = gVideoKeepAspectRatio;
    mIsCloseAble = true;
    mIsOpenGLInited = false;
    mLastGetFrameTime = 0;
}

OpenglView::~OpenglView()
{
    delete ui;
}

void OpenglView::setIsPlaying(bool value)
{
    mIsPlaying = value;

    FunctionTransfer::runInMainThread([=]()
    {
        if (!mIsPlaying)
        {
            ui->pushButton_close->hide();
        }
        update();
    });
}

void OpenglView::setPlayFailed(bool value)
{
    mPlayFailed = value;
    FunctionTransfer::runInMainThread([=]()
    {
        update();
    });
}

void OpenglView::setCameraName(QString name)
{
    mCameraName = name;
    FunctionTransfer::runInMainThread([=]()
    {
        update();
    });
}

void OpenglView::setVideoWidth(int width, int height)
{
    if (width <= 0 || height <= 0) return;

    mWidth = width;
    mHeight = height;
    qDebug()<<__FUNCTION__<<width<<height<<this->isHidden();

    if (mIsOpenGLInited)
    {
        FunctionTransfer::runInMainThread([=]()
        {
            resetGLVertex(this->width(), this->height());
        });
    }
}

void OpenglView::setCloseAble(bool isCloseAble)
{
    mIsCloseAble = isCloseAble;
}

void OpenglView::clear()
{
    FunctionTransfer::runInMainThread([=]()
    {
        mVideoFrame.reset();
        mFaceInfoList.clear();
        update();
    });
}

void OpenglView::enterEvent(QEvent *event)
{
    if (mIsPlaying && mIsCloseAble)
        ui->pushButton_close->show();
}

void OpenglView::leaveEvent(QEvent *event)
{
    ui->pushButton_close->hide();
}

void OpenglView::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && mIsPlaying)
    {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        // 为拖动的鼠标设置一个图片
        QPixmap pixMap  = this->grab();
        QString name = mCameraName;

        if (name.isEmpty())
        {
            name = "drag";
        }
        QString filePath = QString("%1/%2.png").arg("./").arg(name);
        pixMap.save(filePath);

        QList<QUrl> list;
        QUrl url = "file:///" + filePath;
        list.append(url);
        mimeData->setUrls(list);
        drag->setPixmap(pixMap);

        // 实现视频画面拖动，激发拖动事件
        mimeData->setData("playerid", mPlayerId.toUtf8());
        drag->setMimeData(mimeData);
        drag->exec(Qt::CopyAction| Qt::MoveAction);
    }
    else
    {
        QWidget::mouseMoveEvent(event);
    }
}

void OpenglView::onFrameDecoded(VideoFramePtr videoFrame)
{
    FunctionTransfer::runInMainThread([=]()
    {
        int width = videoFrame.get()->width();
        int height = videoFrame.get()->height();
        if (mWidth <= 0 || mHeight <= 0 || mWidth != width || mHeight != height)
        {
            setVideoWidth(width, height);
        }
        mLastGetFrameTime = QDateTime::currentMSecsSinceEpoch();
        mVideoFrame.reset();
        mVideoFrame = videoFrame;
        update(); // 调用update方法将执行paintGL函数
    });
}


void OpenglView::initializeGL()
{
    qDebug()<<__FUNCTION__<<mVideoFrame.get();
    mIsOpenGLInited = true;

    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    // 着色器：就是使用OpenGL着色语言(OpenGL Shading Language, GLSL)编写的一个小函数,
    // GLSL是构成所有OpenGL着色器的语言，具体的GLSL语言的语法需要读者查找相关资料
    // 初始化顶点着色器对象
    pVShader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    // 顶点着色器源码
    const char *vsrc = "attribute vec4 vertexIn; \
        attribute vec2 textureIn; \
        varying vec2 textureOut;  \
        void main(void)           \
    {                         \
            gl_Position = vertexIn; \
            textureOut = textureIn; \
    }";

    // 编译顶点着色器程序
    bool bCompile = pVShader->compileSourceCode(vsrc);
    if (!bCompile)
    {
    }
    // 初始化片段着色器。功能gpu中yuv转换成rgb
    pFShader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    // 片段着色器源码(windows下opengl es 需要加上float这句话)
    const char *fsrc =
#if defined(WIN32)
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
#endif
        "varying vec2 textureOut; \
        uniform sampler2D tex_y; \
        uniform sampler2D tex_u; \
        uniform sampler2D tex_v; \
        void main(void) \
    { \
            vec3 yuv; \
            vec3 rgb; \
            yuv.x = texture2D(tex_y, textureOut).r - 16.0/255.0; \
            yuv.y = texture2D(tex_u, textureOut).g - 0.5; \
            yuv.z = texture2D(tex_v, textureOut).b - 0.5; \
            rgb = mat3( 1.164,       1.164,         1.164, \
                   0,       -0.392,  2.017, \
                   1.596,   -0.813,  0) * yuv; \
            gl_FragColor = vec4(rgb, 1); \
    }";
    // 以上的mat3转换矩阵采用BT601标准版

    // 将glsl源码送入编译器编译着色器程序
    bCompile = pFShader->compileSourceCode(fsrc);
    if (!bCompile)
    {
    }

    // 创建着色器程序容器
    pShaderProgram = new QOpenGLShaderProgram;
    // 将片段着色器添加到程序容器
    pShaderProgram->addShader(pFShader);
    // 将顶点着色器添加到程序容器
    pShaderProgram->addShader(pVShader);
    // 绑定属性vertexIn到指定位置ATTRIB_VERTEX，该属性在顶点着色源码其中有声明
    pShaderProgram->bindAttributeLocation("vertexIn", ATTRIB_VERTEX);
    // 绑定属性textureIn到指定位置ATTRIB_TEXTURE，该属性在顶点着色源码其中有声明
    pShaderProgram->bindAttributeLocation("textureIn", ATTRIB_TEXTURE);
    // 链接所有所有添入到的着色器程序
    pShaderProgram->link();
    // 激活所有链接
    pShaderProgram->bind();
    // 读取着色器中的数据变量tex_y、tex_u、tex_v的位置，这些变量在片段着色器源码中声明
    textureUniformY = pShaderProgram->uniformLocation("tex_y");
    textureUniformU = pShaderProgram->uniformLocation("tex_u");
    textureUniformV = pShaderProgram->uniformLocation("tex_v");

    // 顶点矩阵
    const GLfloat vertexVertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, 1.0f,
    };
    memcpy(m_vertexVertices, vertexVertices, sizeof(vertexVertices));

    // 纹理矩阵
    static const GLfloat textureVertices[] = {
        0.0f,  1.0f,
        1.0f,  1.0f,
        0.0f,  0.0f,
        1.0f,  0.0f,
    };
    // 设置属性ATTRIB_VERTEX的顶点矩阵值以及格式
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, m_vertexVertices);
    // 设置属性ATTRIB_TEXTURE的纹理矩阵值以及格式
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
    // 启用ATTRIB_VERTEX属性的数据，默认是关闭的
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    // 启用ATTRIB_TEXTURE属性的数据，默认是关闭的
    glEnableVertexAttribArray(ATTRIB_TEXTURE);
    // 分别创建y、u、v纹理对象
    pTextureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    pTextureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    pTextureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    pTextureY->create();
    pTextureU->create();
    pTextureV->create();
    glClearColor(0.0, 0.0, 0.0, 0.0); // 将背景色设为黑色
}

void OpenglView::resetGLVertex(int window_W, int window_H)
{
    if (mWidth <= 0 || mHeight <= 0 || !gVideoKeepAspectRatio) // 铺满
    {
        mPicIndexX = 0.0;
        mPicIndexY = 0.0;

        // 顶点矩阵
        const GLfloat vertexVertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f,
        };
        memcpy(m_vertexVertices, vertexVertices, sizeof(vertexVertices));

        // 纹理矩阵
        static const GLfloat textureVertices[] = {
            0.0f,  1.0f,
            1.0f,  1.0f,
            0.0f,  0.0f,
            1.0f,  0.0f,
        };
        // 设置属性ATTRIB_VERTEX的顶点矩阵值以及格式
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, m_vertexVertices);
        // 设置属性ATTRIB_TEXTURE的纹理矩阵值以及格式
        glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
        // 启用ATTRIB_VERTEX属性的数据,默认是关闭的
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        // 启用ATTRIB_TEXTURE属性的数据,默认是关闭的
        glEnableVertexAttribArray(ATTRIB_TEXTURE);
    }
    else // 按比例
    {
        int pix_W = window_W;
        int pix_H = mHeight * pix_W / mWidth;

        int x = this->width() - pix_W;
        int y = this->height() - pix_H;
        x /= 2;
        y /= 2;

        if (y < 0)
        {
            pix_H = window_H;
            pix_W = mWidth * pix_H / mHeight;

            x = this->width() - pix_W;
            y = this->height() - pix_H;
            x /= 2;
            y /= 2;
        }

        mPicIndexX = x * 1.0 / window_W;
        mPicIndexY = y * 1.0 / window_H;

        //qDebug()<<window_W<<window_H<<pix_W<<pix_H<<x<<y;
        float index_y = y *1.0 / window_H * 2.0 -1.0;
        float index_y_1 = index_y * -1.0;
        float index_y_2 = index_y;

        float index_x = x *1.0 / window_W * 2.0 -1.0;
        float index_x_1 = index_x * -1.0;
        float index_x_2 = index_x;

        const GLfloat vertexVertices[] = {
            index_x_2, index_y_2,
            index_x_1,  index_y_2,
            index_x_2, index_y_1,
            index_x_1,  index_y_1,
        };
        memcpy(m_vertexVertices, vertexVertices, sizeof(vertexVertices));

        static const GLfloat textureVertices[] = {
            0.0f,  1.0f,
            1.0f,  1.0f,
            0.0f,  0.0f,
            1.0f,  0.0f,
        };
        // 设置属性ATTRIB_VERTEX的顶点矩阵值以及格式
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, m_vertexVertices);
        // 设置属性ATTRIB_TEXTURE的纹理矩阵值以及格式
        glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
        // 启用ATTRIB_VERTEX属性的数据，默认是关闭的
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        // 启用ATTRIB_TEXTURE属性的数据，默认是关闭的
        glEnableVertexAttribArray(ATTRIB_TEXTURE);
    }
}

void OpenglView::resizeGL(int window_W, int window_H)
{
    mLastGetFrameTime = QDateTime::currentMSecsSinceEpoch();
    if (window_H == 0) // 防止被零除
    {
        window_H = 1; // 将高设为1
    }
    // 设置视口
    glViewport(0, 0, window_W, window_H);

    int x = window_W - ui->pushButton_close->width() - 22;
    int y = 22;
    ui->pushButton_close->move(x, y);

    x = 0;
    y = window_H / 2 - ui->widget_erro->height() / 2;
    ui->widget_erro->move(x, y);
    ui->widget_erro->resize(window_W, ui->widget_erro->height());

    x = 0;
    y = window_H - ui->widget_name->height() - 6;
    ui->widget_name->move(x, y);
    ui->widget_name->resize(window_W, ui->widget_name->height());

    resetGLVertex(window_W, window_H);
}

void OpenglView::paintGL()
{
    mLastGetFrameTime = QDateTime::currentMSecsSinceEpoch();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (ui->pushButton_close->isVisible())
    {
        if (!mIsPlaying || !mIsCloseAble)
        {
            ui->pushButton_close->hide();
        }
    }

    if (!mCameraName.isEmpty() && mIsPlaying)
    {
        ui->widget_name->show();

        QFontMetrics fontMetrics(ui->label_name->font());
        // 获取之前设置的字符串的像素大小
        //int fontSize = fontMetrics.width(mCameraName);
        int fontSize = 20;
        QString str = mCameraName;
        if (fontSize > (this->width() / 2))
        {
            // 返回一个带有省略号的字符串
            str = fontMetrics.elidedText(mCameraName, Qt::ElideRight, (this->width() / 2));
        }
        ui->label_name->setText(str);
        ui->label_name->setToolTip(mCameraName);
    }
    else
    {
        ui->widget_name->hide();
    }

    if (mIsPlaying && mPlayFailed)
    {
        ui->widget_erro->show();
    }
    else
    {
        ui->widget_erro->hide();
    }

    // 如果按比例发生改变，就要重置x、y等变量
    if (mCurrentVideoKeepAspectRatio != gVideoKeepAspectRatio)
    {
        mCurrentVideoKeepAspectRatio = gVideoKeepAspectRatio;
        resetGLVertex(this->width(), this->height());
    }

    VideoFrame * videoFrame = mVideoFrame.get();
    if (videoFrame != NULL)
    {
        uint8_t *bufferYUV = videoFrame->yuvBuffer();
        if (bufferYUV != NULL)
        {
            pShaderProgram->bind(); // 绑定小程序
            // 0对应纹理单元GL_TEXTURE0，1对应纹理单元GL_TEXTURE1，2对应纹理单元GL_TEXTURE2
            glUniform1i(textureUniformY, 0); // 指定y纹理要使用新值
            glUniform1i(textureUniformU, 1); // 指定u纹理要使用新值
            glUniform1i(textureUniformV, 2); // 指定v纹理要使用新值
            glActiveTexture(GL_TEXTURE0); // 激活纹理单元GL_TEXTURE0
            // 使用来自Y分量的数据生成纹理
            glBindTexture(GL_TEXTURE_2D, pTextureY->textureId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, mWidth, mHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bufferYUV);
            glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
            // 使用来自U分量的数据生成纹理
            glBindTexture(GL_TEXTURE_2D, pTextureU->textureId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, mWidth/2, mHeight/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bufferYUV+mWidth*mHeight);
            glActiveTexture(GL_TEXTURE2); // 激活纹理单元GL_TEXTURE2
            // 使用来自V分量的数据生成纹理
            glBindTexture(GL_TEXTURE_2D, pTextureV->textureId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, mWidth/2, mHeight/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bufferYUV+mWidth*mHeight*5/4);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // 使用顶点数组方式绘制图形

            pShaderProgram->release(); // 释放小程序
        }
    }
}
