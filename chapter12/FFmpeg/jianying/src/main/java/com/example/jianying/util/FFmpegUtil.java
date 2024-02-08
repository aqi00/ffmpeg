package com.example.jianying.util;

import android.view.Surface;

public class FFmpegUtil {

    static {
        System.loadLibrary("ffmpeg"); // 加载动态库libffmpeg.so
    }

    public static native void filterVideo(String src_path, String dest_path, String ttf_path, String text_content);

}
