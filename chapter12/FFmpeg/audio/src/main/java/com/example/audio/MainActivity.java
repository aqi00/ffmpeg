package com.example.audio;

import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Toast;

import com.example.audio.util.PermissionUtil;


public class MainActivity extends AppCompatActivity {
    private final static String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        findViewById(R.id.btn_track).setOnClickListener(v -> {
            if (PermissionUtil.checkPermission(this, new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, (int) v.getId() % 65536)) {
                startActivity(new Intent(this, TrackActivity.class));
            }
        });
        findViewById(R.id.btn_opensl).setOnClickListener(v -> {
            if (PermissionUtil.checkPermission(this, new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, (int) v.getId() % 65536)) {
                startActivity(new Intent(this, OpenSLActivity.class));
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == (int) R.id.btn_track % 65536) {
            if (PermissionUtil.checkGrant(grantResults)) { // 用户选择了同意授权
                startActivity(new Intent(this, TrackActivity.class));
            } else {
                Toast.makeText(this, "需要允许存储卡权限才能播放视频噢", Toast.LENGTH_SHORT).show();
            }
        } else if (requestCode == (int) R.id.btn_opensl % 65536) {
            if (PermissionUtil.checkGrant(grantResults)) { // 用户选择了同意授权
                startActivity(new Intent(this, OpenSLActivity.class));
            } else {
                Toast.makeText(this, "需要允许存储卡权限才能播放视频噢", Toast.LENGTH_SHORT).show();
            }
        }
    }

}