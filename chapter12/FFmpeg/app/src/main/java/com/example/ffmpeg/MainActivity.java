package com.example.ffmpeg;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        TextView tv_desc = findViewById(R.id.tv_desc);
        findViewById(R.id.btn_get).setOnClickListener(v -> {
            String desc = "以下为FFmpeg的版本信息：\n"+getFFmpegVersion();
            tv_desc.setText(desc); // 显示从FFmpeg获取的版本信息
        });
    }

    static {
        System.loadLibrary("ffmpeg"); // 加载动态库libffmpeg.so
    }

    // 声明来自原生层的jni函数getFFmpegVersion
    public static native String getFFmpegVersion();

}