package com.example.audio;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;

import com.example.audio.util.FFmpegUtil;
import com.example.audio.util.GetFilePathFromUri;

public class OpenSLActivity extends AppCompatActivity {
    private final static String TAG = "OpenSLActivity";
    private String mAudioPath; // 待播放的音频路径

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_opensl);
        ActivityResultLauncher launcher = registerForActivityResult(new ActivityResultContracts.GetContent(), uri -> {
            if (uri != null) {
                Log.d(TAG, "uri="+uri.toString());
                // 根据Uri获取文件的绝对路径
                mAudioPath = GetFilePathFromUri.getFileAbsolutePath(this, uri);
                Log.d(TAG, "filePath="+mAudioPath);
                new Thread(() -> {
                    FFmpegUtil.playAudioByOpenSL(mAudioPath); // OpenSL播放音频
                }).start();
            } else {
                Log.d(TAG, "uri is null");
            }
        });
        findViewById(R.id.btn_open).setOnClickListener(v -> launcher.launch("audio/*"));
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        FFmpegUtil.stopPlayByOpenSL(); // 停止播放
    }
}