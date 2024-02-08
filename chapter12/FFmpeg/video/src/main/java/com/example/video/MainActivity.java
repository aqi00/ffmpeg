package com.example.video;

import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ConfigurationInfo;
import android.os.Bundle;
import android.widget.Toast;

import com.example.video.util.PermissionUtil;

public class MainActivity extends AppCompatActivity {
    private final static String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        findViewById(R.id.btn_anative).setOnClickListener(v -> {
            if (PermissionUtil.checkPermission(this, new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, (int) v.getId() % 65536)) {
                startActivity(new Intent(this, NativeActivity.class));
            }
        });
        findViewById(R.id.btn_opengl).setOnClickListener(v -> {
            if (PermissionUtil.checkPermission(this, new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, (int) v.getId() % 65536)) {
                startActivity(new Intent(this, OpenGLActivity.class));
            }
        });
        findViewById(R.id.btn_opengles_version).setOnClickListener(v -> showEsVersion());
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == (int) R.id.btn_anative % 65536) {
            if (PermissionUtil.checkGrant(grantResults)) { // 用户选择了同意授权
                startActivity(new Intent(this, NativeActivity.class));
            } else {
                Toast.makeText(this, "需要允许存储卡权限才能播放视频噢", Toast.LENGTH_SHORT).show();
            }
        } else if (requestCode == (int) R.id.btn_opengl % 65536) {
            if (PermissionUtil.checkGrant(grantResults)) { // 用户选择了同意授权
                startActivity(new Intent(this, OpenGLActivity.class));
            } else {
                Toast.makeText(this, "需要允许存储卡权限才能播放视频噢", Toast.LENGTH_SHORT).show();
            }
        }
    }

    // 显示OpenGL ES的版本号
    private void showEsVersion() {
        ActivityManager am = (ActivityManager)
                getSystemService(Context.ACTIVITY_SERVICE);
        ConfigurationInfo info = am.getDeviceConfigurationInfo();
        String versionDesc = String.format("%08X", info.reqGlEsVersion);
        String versionCode = String.format("%d.%d",
                Integer.parseInt(versionDesc)/10000,
                Integer.parseInt(versionDesc)%10000);
        Toast.makeText(this, "当前系统的OpenGL ES版本号为"+versionCode,
                Toast.LENGTH_SHORT).show();
    }

}