#ifndef FFMPEG_VIDEO_COMMON_H
#define FFMPEG_VIDEO_COMMON_H

#include <unistd.h>
#include <stdio.h>
#include <jni.h>
#include <android/log.h>

#define TAG "ffmpeg-video"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

#endif //FFMPEG_VIDEO_COMMON_H
