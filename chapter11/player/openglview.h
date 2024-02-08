#ifndef OPENGLVIEW_H
#define OPENGLVIEW_H

#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QFile>

#include "util/FunctionTransfer.h"
#include "util/VideoFrame.h"

namespace Ui {
class OpenglView;
}

struct FaceInfoNode
{
    QRect faceRect;
};

// 显示视频用的widget（使用OpenGL绘制YUV420P数据）
// 这个仅仅是显示视频画面的控件
class OpenglView : public QOpenGLWidget,protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OpenglView(QWidget *parent = 0);
    ~OpenglView();

    void setPlayerId(QString id){mPlayerId=id;} // 用于协助拖拽 区分是哪个窗口
    QString getPlayerId(){return mPlayerId;}
    void setCloseAble(bool isCloseAble);
    void clear();
    void setIsPlaying(bool value);
    void setPlayFailed(bool value);
    void setCameraName(QString name);
    void setVideoWidth(int width, int height);
    qint64 getLastGetFrameTime(){return mLastGetFrameTime;}
    void onFrameDecoded(VideoFramePtr videoFrame);

signals:
    void sig_CloseBtnClick();
    void sig_Drag(QString id_from, QString id_to);

protected:
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

private:
    bool gVideoKeepAspectRatio = true;
    bool mIsPlaying; // 是否正在播放
    bool mPlayFailed; // 是否播放失败
    bool mIsCloseAble; // 是否显示关闭按钮

    QString mCameraName;
    qint64 mLastGetFrameTime; //上一次获取到帧的时间戳
    void resetGLVertex(int window_W, int window_H);

protected:
    void initializeGL() Q_DECL_OVERRIDE;
    void resizeGL(int window_W, int window_H) Q_DECL_OVERRIDE;
    void paintGL() Q_DECL_OVERRIDE;

private:
    // 用于OpenGL绘制图像
    GLuint textureUniformY; // y纹理数据位置
    GLuint textureUniformU; // u纹理数据位置
    GLuint textureUniformV; // v纹理数据位置
    QOpenGLTexture *pTextureY;  // y纹理对象
    QOpenGLTexture *pTextureU;  // u纹理对象
    QOpenGLTexture *pTextureV;  // v纹理对象
    QOpenGLShader *pVShader;  // 顶点着色器程序对象
    QOpenGLShader *pFShader;  // 片段着色器对象
    QOpenGLShaderProgram *pShaderProgram; // 着色器程序容器
    GLfloat *m_vertexVertices; // 顶点矩阵

    float mPicIndexX; // 按比例显示情况下 图像偏移量百分比 (相对于窗口大小的)
    float mPicIndexY; //
    int mWidth; // 视频宽度
    int mHeight; // 视频高度

    VideoFramePtr mVideoFrame;
    QList<FaceInfoNode> mFaceInfoList;

    bool mIsOpenGLInited; // OpenGL初始化函数是否执行过了
    // 当前模式是否是按比例 当检测到与全局变量不一致的时候 则重新设置OpenGL矩阵
    bool mCurrentVideoKeepAspectRatio;
    QString mPlayerId;

private:
    Ui::OpenglView *ui;
};

#endif // OPENGLVIEW_H
