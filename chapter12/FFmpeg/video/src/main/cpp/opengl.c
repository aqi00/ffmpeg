
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>

#include "common.h"
#include "opengl.h"

#define GET_STR(x) #x

#ifdef __cplusplus
extern "C" {
#endif

int is_stop = 0; // 是否停止播放。0 不停止；1 停止

static const char *vertexShader = GET_STR(
        attribute vec4 aPosition; // 顶点坐标，在外部获取传递进来
        attribute vec2 aTexCoord; // 材质（纹理）顶点坐标
        varying vec2 vTexCoord;   // 输出的材质（纹理）坐标，给片元着色器使用
        void main() {
            // 纹理坐标转换，以左上角为原点的纹理坐标转换成以左下角为原点的纹理坐标，
            // 比如以左上角为原点的（0，0）对应以左下角为原点的纹理坐标的（0，1）
            vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
            gl_Position = aPosition;
        }
);

static const char *fragYUV420P = GET_STR(
        precision mediump float;    // 精度
        varying vec2 vTexCoord;     // 顶点着色器传递的坐标，相同名字opengl会自动关联
        uniform sampler2D yTexture; // 输入的材质（不透明灰度，单像素）
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        void main() {
            vec3 yuv;
            vec3 rgb;
            yuv.x = texture2D(yTexture, vTexCoord).r - 16.0/255.0; // y分量
            // 因为UV的默认值是127，所以我们这里要减去0.5（OpenGLES的Shader中会把内存中0～255的整数数值换算为0.0～1.0的浮点数值）
            yuv.y = texture2D(uTexture, vTexCoord).r - 0.5; // u分量
            yuv.z = texture2D(vTexture, vTexCoord).r - 0.5; // v分量
            // yuv转换成rgb，两种方法，一种是RGB按照特定换算公式单独转换
            // 另外一种是使用矩阵转换
            rgb = mat3(1.164,       1.164,         1.164,
                       0,       -0.392,  2.017,
                       1.596,   -0.813,  0) * yuv;
            // 输出像素颜色
            gl_FragColor = vec4(rgb, 1.0);
        }
);

GLint InitShader(const char *code, GLint type) {
    // 创建shader
    GLint sh = glCreateShader(type);
    if (sh == 0) {
        LOGE("glCreateShader %d failed!", type);
        return 0;
    }
    // 加载shader
    glShaderSource(sh,
                   1,    //shader数量
                   &code, //shader代码
                   0);   //代码长度
    // 编译shader
    glCompileShader(sh);
    // 获取编译情况
    GLint status;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOGE("glCompileShader failed!");
        return 0;
    }
    LOGE("glCompileShader success!");
    return sh;
}

void play_video(JNIEnv *env, jclass clazz, jstring video_path, jobject surface) {
    const char *videoPath = (*env)->GetStringUTFChars(env, video_path, 0);
    LOGE("PlayVideo: %s", videoPath);
    if (videoPath == NULL) {
        LOGE("videoPath is null");
        return;
    }
    is_stop = 0;

    AVFormatContext *fmt_ctx = avformat_alloc_context();
    LOGI("Open video file");
    // 打开音视频文件
    if (avformat_open_input(&fmt_ctx, videoPath, NULL, NULL) != 0) {
        LOGE("Cannot open video file: %s\n", videoPath);
        return;
    }

    LOGI("Retrieve stream information");
    // 查找音视频文件中的流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Cannot find stream information.");
        return;
    }

    LOGI("Find video stream");
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index == -1) {
        LOGE("No video stream found.");
        return;
    }
    int fps = av_q2d(fmt_ctx->streams[video_index]->r_frame_rate); // 帧率
    int interval = 1000 * 1000 / fps; // 根据帧率计算每帧之间的播放间隔
    LOGE("fps=%d, interval=%d", fps, interval);

    AVCodecParameters *codec_para = fmt_ctx->streams[video_index]->codecpar;
    LOGI("Find the decoder for the video stream");
    // 查找视频解码器
    AVCodec *codec = (AVCodec*) avcodec_find_decoder(codec_para->codec_id);
    if (codec == NULL) {
        LOGE("Codec not found.");
        return;
    }

    // 分配解码器的实例
    AVCodecContext *decode_ctx = avcodec_alloc_context3(codec);
    if (decode_ctx == NULL) {
        LOGE("CodecContext not found.");
        return;
    }

    // 把视频流中的编解码参数复制给解码器的实例
    if (avcodec_parameters_to_context(decode_ctx, codec_para) < 0) {
        LOGE("Fill CodecContext failed.");
        return;
    }

    LOGI("open Codec");
    // 打开解码器的实例
    if (avcodec_open2(decode_ctx, codec, NULL)) {
        LOGE("Open CodecContext failed.");
        return;
    }

    enum AVPixelFormat dest_format = AV_PIX_FMT_YUV420P;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    AVFrame *render_frame = av_frame_alloc(); // 分配一个渲染帧

    LOGI("Determine required buffer size and allocate buffer");
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(render_frame->data, render_frame->linesize,
                   decode_ctx->width, decode_ctx->height, dest_format, 1);

    LOGI("init SwsContext");
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    struct SwsContext *swsContext = sws_getContext(
            decode_ctx->width, decode_ctx->height, decode_ctx->pix_fmt,
            decode_ctx->width, decode_ctx->height, dest_format,
            SWS_BILINEAR, NULL, NULL, NULL);
    if (swsContext == NULL) {
        LOGE("Init SwsContext failed.");
        return;
    }

    int videoWidth = decode_ctx->width;
    int videoHeight = decode_ctx->height;
    LOGI("VideoSize: [%d,%d]", videoWidth, videoHeight);

    // 获取EGL显示器
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
         goto __ERROR;
    }

    // 初始化EGL显示器
    if (EGL_TRUE != eglInitialize(display, 0, 0)) {
        LOGE("eglGetDisplay init failed");
        goto __ERROR;
    }

    EGLConfig config; // EGL配置
    EGLint config_num; // 配置数量
    EGLint config_spec[] = { // 配置规格，涉及RGB颜色空间
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
    };
    // 给EGL显示器选择最佳配置
    if (EGL_TRUE != eglChooseConfig(display, config_spec, &config, 1, &config_num)) {
        LOGE("eglChooseConfig failed!");
        goto __ERROR;
    }
    // 从表面对象获取原生窗口
    ANativeWindow *nativeWindows = ANativeWindow_fromSurface(env, surface);

    // 创建EGL表面。这里EGL接管了原生窗口的表面对象
    EGLSurface winSurface = eglCreateWindowSurface(display, config, nativeWindows, 0);
    if (winSurface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed!");
        goto __ERROR;
    }

    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
    };
    // 结合EGL显示器与EGL配置创建EGL实例
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed!");
        goto __ERROR;
    }
    // 创建EGL环境，之后就可以执行OpenGL ES的相关操作了
    if (EGL_TRUE != eglMakeCurrent(display, winSurface, winSurface, context)) {
        LOGE("eglMakeCurrent failed!");
        goto __ERROR;
    }

    /////////////////////////////////////////////////////////////
    // 创建渲染程序
    GLint program = glCreateProgram();
    if (program == 0) {
        LOGE("glCreateProgram failed!");
        return;
    }

    // 顶点shader初始化
    GLint vsh = InitShader(vertexShader, GL_VERTEX_SHADER);
    // 片元yuv420 shader初始化
    GLint fsh = InitShader(fragYUV420P, GL_FRAGMENT_SHADER);

    // 渲染程序中加入着色器代码
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);

    // 链接程序
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        LOGE("glLinkProgram failed!");
        return;
    }
    glUseProgram(program);
    LOGE("glLinkProgram success!");
    /////////////////////////////////////////////////////////////

    // 加入三维顶点数据，两个三角形组成正方形
    static float vers[] = {
            1.0f, -1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f,
    };
    GLuint apos = (GLuint) glGetAttribLocation(program, "aPosition");
    glEnableVertexAttribArray(apos);
    // 传递顶点
    glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 12, vers);

    // 加入材质坐标数据
    static float txts[] = {
            1.0f, 0.0f, //右下
            0.0f, 0.0f,
            1.0f, 1.0f,
            0.0, 1.0
    };
    GLuint atex = (GLuint) glGetAttribLocation(program, "aTexCoord");
    glEnableVertexAttribArray(atex);
    glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 8, txts);

    // 设置纹理层
    glUniform1i(glGetUniformLocation(program, "yTexture"), 0); // 第1层纹理
    glUniform1i(glGetUniformLocation(program, "uTexture"), 1); // 第2层纹理
    glUniform1i(glGetUniformLocation(program, "vTexture"), 2); // 第3层纹理

    // 创建OpenGL纹理
    GLuint texts[3] = {0};
    // 创建三个纹理
    glGenTextures(3, texts);
    // 绑定指定纹理
    glBindTexture(GL_TEXTURE_2D, texts[0]);
    // 缩小的过滤器
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // 设置纹理的格式和宽高
    glTexImage2D(GL_TEXTURE_2D,
                 0,           // 细节基本 0默认
                 GL_LUMINANCE, // gpu内部格式 亮度，灰度图
                 videoWidth, videoHeight, // 拉升到全屏
                 0,              // 边框
                 GL_LUMINANCE,   // 数据的像素格式
                 GL_UNSIGNED_BYTE, // 像素的数据类型
                 NULL             // 纹理的数据
    );

    // 绑定指定纹理
    glBindTexture(GL_TEXTURE_2D, texts[1]);
    // 缩小的过滤器
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // 设置纹理的格式和宽高
    glTexImage2D(GL_TEXTURE_2D,
                 0,           // 细节基本 0默认
                 GL_LUMINANCE, // gpu内部格式 亮度，灰度图
                 videoWidth / 2, videoHeight / 2, // 拉升到全屏
                 0,              // 边框
                 GL_LUMINANCE,   // 数据的像素格式
                 GL_UNSIGNED_BYTE, // 像素的数据类型
                 NULL             // 纹理的数据
    );

    // 绑定指定纹理
    glBindTexture(GL_TEXTURE_2D, texts[2]);
    // 缩小的过滤器
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // 设置纹理的格式和宽高
    glTexImage2D(GL_TEXTURE_2D,
                 0,           // 细节基本 0默认
                 GL_LUMINANCE, // gpu内部格式 亮度，灰度图
                 videoWidth / 2, videoHeight / 2, // 拉升到全屏
                 0,              // 边框
                 GL_LUMINANCE,   // 数据的像素格式
                 GL_UNSIGNED_BYTE, // 像素的数据类型
                 NULL             // 纹理的数据
    );

    //////////////////////////////////////////////////////
    // 纹理的修改和显示
    unsigned char *buf[3] = {0};
    buf[0] = malloc(videoWidth * videoHeight);
    buf[1] = malloc(videoWidth * videoHeight/4);
    buf[2] = malloc(videoWidth * videoHeight/4);

    LOGI("read frame");
    long play_time = av_gettime(); // 各帧的约定播放时间点
    long now_time = av_gettime(); // 当前时间点
    while (av_read_frame(fmt_ctx, packet) == 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要解码
            // 把未解压的数据包发给解码器实例
            int ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0) {
                LOGE("向解码器-发送数据");
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(decode_ctx, frame);
                if (ret == 0) {
                    LOGE("从解码器-接收数据");
                    // 开始转换图像格式
                    sws_scale(swsContext, (uint8_t const *const *) frame->data,
                              frame->linesize, 0, decode_ctx->height,
                              render_frame->data, render_frame->linesize);

                    buf[0] = frame->data[0];
                    memcpy(buf[0],frame->data[0],videoWidth*videoHeight); // Y分量
                    memcpy(buf[1],frame->data[1],videoWidth*videoHeight/4); // U分量
                    memcpy(buf[2],frame->data[2],videoWidth*videoHeight/4); // V分量

                    // 激活第1层纹理，绑定到创建的OpenGL纹理
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D,texts[0]);
                    // 替换纹理内容
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,videoWidth,videoHeight,GL_LUMINANCE,GL_UNSIGNED_BYTE,buf[0]);

                    // 激活第2层纹理，绑定到创建的OpenGL纹理
                    glActiveTexture(GL_TEXTURE0+1);
                    glBindTexture(GL_TEXTURE_2D,texts[1]);
                    // 替换纹理内容
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,videoWidth/2,videoHeight/2,GL_LUMINANCE,GL_UNSIGNED_BYTE,buf[1]);

                    // 激活第2层纹理，绑定到创建的OpenGL纹理
                    glActiveTexture(GL_TEXTURE0+2);
                    glBindTexture(GL_TEXTURE_2D,texts[2]);
                    // 替换纹理内容
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,videoWidth/2,videoHeight/2,GL_LUMINANCE,GL_UNSIGNED_BYTE,buf[2]);
                    // 三维绘制
                    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
                    // 把OpenGL ES的纹理缓存显示到屏幕上
                    eglSwapBuffers(display, winSurface);
                    now_time = av_gettime();
                    play_time += interval; // 下一帧的约定播放时间点
                    long temp_interval = play_time-now_time;
                    temp_interval = (temp_interval < 0) ? 0 : temp_interval;
                    LOGE("interval=%lld, temp_interval=%lld", interval, temp_interval);
                    av_usleep(temp_interval); // 睡眠若干微秒
                    if (is_stop) { // 是否停止播放
                        break;
                    }
                } else {
                    LOGE("从解码器-接收-数据失败：%d", ret);
                }
            } else {
                LOGE("向解码器-发送-数据失败：%d", ret);
            }
        }
        av_packet_unref(packet);
    }

    __ERROR:
    LOGI("release memory");
    // 释放各类资源
    if(nativeWindows != NULL){
        ANativeWindow_release(nativeWindows); // 释放原生窗口
    }
    if (winSurface != EGL_NO_SURFACE) {
        eglDestroySurface(display, winSurface); // 销毁EGL表面
    }
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context); // 销毁EGL实例
    }

    av_frame_free(&frame);
    av_frame_free(&render_frame);
    av_packet_free(&packet);
    avcodec_close(decode_ctx);
    avcodec_free_context(&decode_ctx);
    avformat_close_input(&fmt_ctx);
    avformat_free_context(fmt_ctx);
    (*env)->ReleaseStringUTFChars(env, video_path, videoPath);
}

#ifdef __cplusplus
}
#endif