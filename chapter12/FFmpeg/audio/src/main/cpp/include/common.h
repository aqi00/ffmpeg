#ifndef FFMPEG_AUDIO_COMMON_H
#define FFMPEG_AUDIO_COMMON_H

#include <unistd.h>
#include <stdio.h>
#include <jni.h>
#include <android/log.h>

#define TAG "ffmpeg-audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

extern int is_stop; // 是否停止播放。0 不停止；1 停止
void play_video(JNIEnv *env, jclass clazz, jstring video_path, jobject surface);

#ifdef __cplusplus
}
#endif

#endif //FFMPEG_AUDIO_COMMON_H
