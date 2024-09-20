package com.example.exoplayer;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.widget.MediaController;
import android.widget.Toast;
import android.widget.VideoView;

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.audio.AudioProcessor;
import androidx.media3.common.audio.ChannelMixingAudioProcessor;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.effect.Crop;
import androidx.media3.effect.ScaleAndRotateTransformation;
import androidx.media3.transformer.Composition;
import androidx.media3.transformer.EditedMediaItem;
import androidx.media3.transformer.Effects;
import androidx.media3.transformer.ExportException;
import androidx.media3.transformer.ExportResult;
import androidx.media3.transformer.Transformer;

import com.example.exoplayer.util.DateUtil;
import com.google.common.collect.ImmutableList;

@UnstableApi
public class VideoEditActivity extends AppCompatActivity {
    private final static String TAG = "VideoPlayActivity";
    private VideoView vv_content; // 声明一个视频视图对象
    private Uri mVideoUri;
    private String mPath;
    private Context mContext;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video_edit);
        mContext = this;
        // 从布局文件中获取名叫vv_content的视频视图
        vv_content = findViewById(R.id.vv_content);
        // 注册一个善后工作的活动结果启动器，获取指定类型的内容
        ActivityResultLauncher launcher = registerForActivityResult(
                new ActivityResultContracts.GetContent(), uri -> {
                    if (uri != null) {
                        mVideoUri = uri;
                        playVideo(uri); // 播放视频
                    }
                });
        findViewById(R.id.btn_choose).setOnClickListener(v -> launcher.launch("video/*"));
        mPath = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS).toString() + '/';
        findViewById(R.id.btn_edit).setOnClickListener(v -> {
            MediaItem.ClippingConfiguration clippingConfiguration = new MediaItem.ClippingConfiguration.Builder()
                    .setStartPositionMs(10_000) // start at 10 seconds
                    .setEndPositionMs(20_000) // end at 20 seconds
                    .build();
            MediaItem mediaItem = new MediaItem.Builder()
                    .setUri(mVideoUri)
                    .setClippingConfiguration(clippingConfiguration)
                    .build();
            Transformer transformer = new Transformer.Builder(this)
                    .setVideoMimeType(MimeTypes.VIDEO_H265)
                    .setAudioMimeType(MimeTypes.AUDIO_AAC)
                    .addListener(new Transformer.Listener() {
                        @Override
                        public void onCompleted(Composition composition, ExportResult exportResult) {
                            //Transformer.Listener.super.onCompleted(composition, exportResult);
                            Toast.makeText(mContext, "转换成功", Toast.LENGTH_SHORT).show();
                        }

                        @Override
                        public void onError(Composition composition, ExportResult exportResult, ExportException exportException) {
                            //Transformer.Listener.super.onError(composition, exportResult, exportException);
                            Toast.makeText(mContext, "转换失败", Toast.LENGTH_SHORT).show();
                            Log.d(TAG, "exportException: "+exportException.toString());
                        }
                    })
                    .build();
            String outputPath = mPath + DateUtil.getNowDateTime() + ".mp4";

            //transformer.start(mediaItem, outputPath);

            ScaleAndRotateTransformation rotateEffect = new ScaleAndRotateTransformation.Builder()
                    //.setRotationDegrees(90f)
                    .setScale(0.5f, 0.5f)
                    .build();
            //Crop cropEffect = new Crop(-0.5f, 0.5f, -0.5f, 0.5f);
            //ChannelMixingAudioProcessor会报错：androidx.media3.transformer.ExportException: Audio error: Error while registering input 0, audioFormat=AudioFormat[sampleRate=48000, channelCount=2, encoding=2]
            //ChannelMixingAudioProcessor channelMixingProcessor = new ChannelMixingAudioProcessor();
            Effects effects = new Effects(
                    ImmutableList.of(),
                    ImmutableList.of(rotateEffect)
                    //ImmutableList.of(rotateEffect, cropEffect)
            );
            EditedMediaItem editedMediaItem = new EditedMediaItem.Builder(mediaItem)
                    .setEffects(effects)
                    .build();
            transformer.start(editedMediaItem, outputPath);

        });
    }

    private void playVideo(Uri uri) {
        vv_content.setVideoURI(uri); // 设置视频视图的视频路径
        MediaController mc = new MediaController(this); // 创建一个媒体控制条
        vv_content.setMediaController(mc); // 给视频视图设置相关联的媒体控制条
        mc.setMediaPlayer(vv_content); // 给媒体控制条设置相关联的视频视图
        vv_content.start(); // 视频视图开始播放
    }

}