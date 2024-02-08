package com.example.video;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;

import com.example.video.widget.FFmpegView;
import com.example.video.util.GetFilePathFromUri;

public class NativeActivity extends AppCompatActivity {
    private final static String TAG = "NativeActivity";
    private FFmpegView fv_video; // FFmpeg的播放视图
    private String mVideoPath; // 待播放的视频路径

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_native);
        fv_video = findViewById(R.id.fv_video);
        ActivityResultLauncher launcher = registerForActivityResult(new ActivityResultContracts.GetContent(), uri -> {
            if (uri != null) {
                Log.d(TAG, "uri="+uri.toString());
                // 根据Uri获取文件的绝对路径
                mVideoPath = GetFilePathFromUri.getFileAbsolutePath(this, uri);
                Log.d(TAG, "filePath="+mVideoPath);
                fv_video.playVideoByNative(mVideoPath); // ANative方式播放
            } else {
                Log.d(TAG, "uri is null");
            }
        });
        findViewById(R.id.btn_open).setOnClickListener(v -> launcher.launch("video/*"));
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        fv_video.stopPlay(); // 停止播放
    }
}