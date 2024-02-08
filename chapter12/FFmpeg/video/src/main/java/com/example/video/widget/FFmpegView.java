package com.example.video.widget;

import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;

import com.example.video.util.FFmpegUtil;

public class FFmpegView extends SurfaceView {
    private final static String TAG = "FFmpegView";
    private Surface mSurface; // 声明一个表面对象

    public FFmpegView(Context context) {
        super(context);
        init();
    }

    public FFmpegView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public FFmpegView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        getHolder().setFormat(PixelFormat.RGBA_8888);
        mSurface = getHolder().getSurface(); // 获取表面对象
    }

    public void playVideoByNative(final String videoPath) {
        Log.d(TAG, "run: playVideoByNative");
        new Thread(() -> {
            FFmpegUtil.playVideoByNative(videoPath, mSurface);
        }).start();
    }

    public void playVideoByOpenGL(final String videoPath) {
        Log.d(TAG, "run: playVideoByOpenGL");
        new Thread(() -> { // 开始播放
            FFmpegUtil.playVideoByOpenGL(videoPath, mSurface);
        }).start();
    }

    public void stopPlay() {
        Log.d(TAG, "run: stopPlay");
        FFmpegUtil.stopPlay(); // 停止播放
    }
}
