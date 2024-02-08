#ifndef FFMPEG_VIDEO_OPENGL_H
#define FFMPEG_VIDEO_OPENGL_H

#include <unistd.h>
#include <stdio.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int is_stop; // 是否停止播放。0 不停止；1 停止
void play_video(JNIEnv *env, jclass clazz, jstring video_path, jobject surface);

#ifdef __cplusplus
}
#endif

#endif //FFMPEG_VIDEO_OPENGL_H
