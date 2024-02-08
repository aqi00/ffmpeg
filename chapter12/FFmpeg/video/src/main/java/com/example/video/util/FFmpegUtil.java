package com.example.video.util;

import android.view.Surface;

public class FFmpegUtil {

    static {
        System.loadLibrary("ffmpeg"); // 加载动态库libffmpeg.so
    }

    public static native void playVideoByNative(String videoPath, Surface surface);

    public static native void playVideoByOpenGL(String videoPath, Surface surface);

    public static native void stopPlay();

}
