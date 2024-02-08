package com.example.jianying;

import androidx.appcompat.app.AppCompatActivity;

import android.net.Uri;
import android.os.Bundle;
import android.widget.Button;
import android.widget.MediaController;
import android.widget.VideoView;

public class PlayerActivity extends AppCompatActivity {
    private String mVideoPath; // 视频文件的路径
    private VideoView vv_content; // 视频视图
    private boolean is_pause = false; // 是否暂停

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_player);
        mVideoPath = getIntent().getStringExtra("video_path");
        vv_content = findViewById(R.id.vv_content);
        Button btn_pause = findViewById(R.id.btn_pause);
        btn_pause.setOnClickListener(v -> {
            is_pause = !is_pause;
            if (is_pause) {
                btn_pause.setText("恢复播放");
                vv_content.pause(); // 暂停播放
            } else {
                btn_pause.setText("暂停播放");
                vv_content.start(); // 恢复播放
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!is_pause) {
            if (vv_content.isPlaying()) {
                vv_content.start(); // 恢复播放
            } else {
                vv_content.setVideoURI(Uri.parse(mVideoPath)); // 设置视频视图的视频路径
                MediaController mc = new MediaController(this); // 创建一个媒体控制条
                vv_content.setMediaController(mc); // 给视频视图设置相关联的媒体控制条
                mc.setMediaPlayer(vv_content); // 给媒体控制条设置相关联的视频视图
                vv_content.start(); // 视频视图开始播放
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        vv_content.pause(); // 暂停播放
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        vv_content.stopPlayback(); // 停止播放
    }
}