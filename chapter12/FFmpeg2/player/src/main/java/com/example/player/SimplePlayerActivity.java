package com.example.player;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;

import com.shuyu.gsyvideoplayer.utils.OrientationUtils;
import com.shuyu.gsyvideoplayer.video.NormalGSYVideoPlayer;

public class SimplePlayerActivity extends AppCompatActivity {
    private static String URL_MP4 = "https://video.zohi.tv/fs/transcode/20240520/8cc/355193-1716184798-transv.mp4";
    private static String URL_LIVE = "https://tmpstream.hyrtv.cn/xwzh/sd/live.m3u8";
    private NormalGSYVideoPlayer video_player;
    private OrientationUtils orientationUtils;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_simple_player);
        video_player = findViewById(R.id.video_player);
        // 设置旋转
        orientationUtils = new OrientationUtils(this, video_player);
        // 设置全屏按键功能,这是使用的是选择屏幕，而不是全屏
        video_player.getFullscreenButton().setOnClickListener(v -> {
            // 不需要屏幕旋转，还需要设置 setNeedOrientationUtils(false)
            orientationUtils.resolveByClick();
        });
        // 不需要屏幕旋转
        video_player.setNeedOrientationUtils(false);
        findViewById(R.id.btn_play_mp4).setOnClickListener(v -> {
            video_player.setUp(URL_MP4, true, "数字中国峰会迎宾曲");
        });
        findViewById(R.id.btn_play_live).setOnClickListener(v -> {
            video_player.setUp(URL_LIVE, true, "河源电视台");
        });
    }
}