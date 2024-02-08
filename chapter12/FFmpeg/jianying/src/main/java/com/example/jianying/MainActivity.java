package com.example.jianying;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import com.example.jianying.util.FFmpegUtil;
import com.example.jianying.util.FileUtil;
import com.example.jianying.util.GetFilePathFromUri;
import com.example.jianying.util.PermissionUtil;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    private final static String TAG = "MainActivity";
    private final static String SONTI = "simsun.ttc";
    private final static String MAITI = "simkai.ttf";
    private Handler mHandler = new Handler(Looper.myLooper());
    private String src_path; // 输入视频的来源路径
    private String dest_path; // 输出视频的目标路径
    private String storage_path; // 视频文件的存储目录
    private int font_type = 1; // 1为宋体，2为楷体

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        storage_path = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS).toString();
        dest_path = storage_path + File.separator + "dest.mp4";
        EditText et_text = findViewById(R.id.et_text);
        TextView tv_src = findViewById(R.id.tv_src);
        TextView tv_dest = findViewById(R.id.tv_dest);
        RadioGroup rg_font = findViewById(R.id.rg_font);
        rg_font.setOnCheckedChangeListener((group, checkedId) -> {
            font_type = (checkedId==R.id.rb_songti) ? 1 : 2; // 切换字体类型
        });
        ActivityResultLauncher launcher = registerForActivityResult(new ActivityResultContracts.GetContent(), uri -> {
            if (uri != null) {
                Log.d(TAG, "uri="+uri.toString());
                // 根据Uri获取文件的绝对路径
                src_path = GetFilePathFromUri.getFileAbsolutePath(this, uri);
                Log.d(TAG, "filePath="+src_path);
                tv_src.setText("待加工的视频文件为"+src_path);
            } else {
                Log.d(TAG, "uri is null");
            }
        });
        findViewById(R.id.btn_open).setOnClickListener(v -> launcher.launch("video/*"));
        findViewById(R.id.btn_filter).setOnClickListener(v -> {
            if (TextUtils.isEmpty(src_path)) {
                Toast.makeText(this, "请先选择视频文件", Toast.LENGTH_SHORT).show();
                return;
            }
            String text = et_text.getText().toString();
            if (TextUtils.isEmpty(text)) {
                Toast.makeText(this, "请先输入水印文字", Toast.LENGTH_SHORT).show();
                return;
            }
            Toast.makeText(this, "正在加工视频文件，请耐心等待", Toast.LENGTH_SHORT).show();
            String ttf_path = storage_path + File.separator + (font_type==1?SONTI:MAITI);
            FFmpegUtil.filterVideo(src_path, dest_path, ttf_path, "@ "+text); // 加工视频
            Toast.makeText(this, "视频文件加工完毕", Toast.LENGTH_SHORT).show();
            tv_dest.setText("加工后的视频文件为"+dest_path);
        });
        findViewById(R.id.btn_play).setOnClickListener(v -> {
            if (!FileUtil.isExists(dest_path)) {
                Toast.makeText(this, "请先加工视频文件", Toast.LENGTH_SHORT).show();
                return;
            }
            Intent intent = new Intent(this, PlayerActivity.class);
            intent.putExtra("video_path", dest_path);
            startActivity(intent); // 打开播放页面
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        mHandler.postDelayed(() -> {
            if (PermissionUtil.checkPermission(this, new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, (int) R.id.btn_open % 65536)) {
                copyFontFile(); // 把字体文件复制到应用的私有目录
            }
        }, 200);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == (int) R.id.btn_open % 65536) {
            if (PermissionUtil.checkGrant(grantResults)) { // 用户选择了同意授权
                copyFontFile(); // 把字体文件复制到应用的私有目录
            } else {
                Toast.makeText(this, "需要允许存储卡权限才能加工视频噢", Toast.LENGTH_SHORT).show();
            }
        }
    }

    // 把字体文件复制到应用的私有目录
    private void copyFontFile() {
        FileUtil.copyFileFromRaw(this, R.raw.simsun, SONTI, storage_path); // 复制宋体文件
        FileUtil.copyFileFromRaw(this, R.raw.simkai, MAITI, storage_path); // 复制楷体文件
    }

}