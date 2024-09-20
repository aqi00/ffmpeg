package com.example.exoplayer;

import android.os.Bundle;

import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.Player;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.DataSource;
import androidx.media3.datasource.DefaultDataSource;
import androidx.media3.datasource.rtmp.RtmpDataSource;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.hls.HlsMediaSource;
import androidx.media3.exoplayer.rtsp.RtspMediaSource;
import androidx.media3.exoplayer.source.MediaSource;
import androidx.media3.exoplayer.source.MergingMediaSource;
import androidx.media3.exoplayer.source.ProgressiveMediaSource;
import androidx.media3.exoplayer.source.SingleSampleMediaSource;
import androidx.media3.ui.PlayerView;

import java.util.Locale;

@UnstableApi
public class ExoPlayerActivity extends AppCompatActivity {

    private final static String TAG = "ExoPlayerActivity";
    //private final static String URL_HLS = "https://storage.googleapis.com/exoplayer-test-media-0/BigBuckBunny_320x180.mp4";
    //private final static String URL_HLS = "https://tmpstream.hyrtv.cn/xwzh/sd/live.m3u8";

    // ZLMediaKit
//    private final static String URL_HLS = "http://124.70.221.25:8080/live/test/hls.m3u8";
//    private final static String URL_RTMP = "rtmp://124.70.221.25/live/test";

    // SRS
    private final static String URL_HLS = "http://124.70.221.25:8080/live/test.m3u8";
    private final static String URL_RTSP= "rtsp://124.70.221.25/live/test";
    private final static String URL_RTMP= "rtmp://124.70.221.25/live/test";

    //    private final static String URL_VIDEO = UrlConstant.HTTP_PREFIX + "海洋世界.mp4";
//    private final static String URL_SUBTITLE = UrlConstant.HTTP_PREFIX + "海洋世界.srt";
    private ExoPlayer mPlayer; // 声明一个新型播放器对象

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_exo_player);
        PlayerView pv_content = findViewById(R.id.pv_content);
        // 注册一个善后工作的活动结果启动器，获取指定类型的内容
        ActivityResultLauncher launcher = registerForActivityResult(
                new ActivityResultContracts.GetContent(), uri -> {
                    if (uri != null) {
                        playVideo(uri); // 播放视频
                    }
                });
        findViewById(R.id.btn_play_local).setOnClickListener(v -> launcher.launch("video/*"));
        findViewById(R.id.btn_play_hls).setOnClickListener(v -> playVideo(Uri.parse(URL_HLS)));
        findViewById(R.id.btn_play_rtmp).setOnClickListener(v -> playVideo(Uri.parse(URL_RTMP)));
        findViewById(R.id.btn_play_subtitle).setOnClickListener(v -> {
//            Uri videoUri = Uri.parse(URL_VIDEO);
//            Uri subtitleUri = Uri.parse(URL_SUBTITLE);
//            playVideoWithSubtitle(videoUri, subtitleUri); // 播放带字幕的视频
        });
        mPlayer = new ExoPlayer.Builder(this).build();
        pv_content.setPlayer(mPlayer); // 设置播放器视图的播放器对象
        initWelcomeSpinner(); // 初始化迎宾曲下拉框
    }

    // 播放视频
    private void playVideo(Uri uri) {
        DataSource.Factory factory = new DefaultDataSource.Factory(this);
        // 创建指定地址的媒体对象
        MediaItem videoItem = new MediaItem.Builder().setUri(uri).build();
        // 基于工厂对象和媒体对象创建媒体来源
        MediaSource videoSource;
        if (uri.getPath().endsWith("m3u8")) { // hls链接
            videoSource = new HlsMediaSource.Factory(factory)
                    .createMediaSource(videoItem);
        } else if (uri.getPath().startsWith("rtsp")) { // rtsp链接
            videoSource = new RtspMediaSource.Factory()
                    .createMediaSource(videoItem);
        } else if (uri.getPath().startsWith("rtmp")) { // rtmp链接
            videoSource = new ProgressiveMediaSource.Factory(new RtmpDataSource.Factory())
                    .createMediaSource(videoItem);
        } else { // 其他链接（http开头或https开头的普通视频链接）
            videoSource = new ProgressiveMediaSource.Factory(factory)
                    .createMediaSource(videoItem);
        }
        mPlayer.setMediaSource(videoSource); // 设置播放器的媒体来源
        // 给播放器添加事件监听器
        mPlayer.addListener(new Player.Listener() {
            @Override
            public void onPlaybackStateChanged(int state) {
                if (state == Player.STATE_BUFFERING) { // 视频正在缓冲
                    Log.d(TAG, "视频正在缓冲");
                } else if (state == Player.STATE_READY) { // 视频准备就绪
                    Log.d(TAG, "视频准备就绪");
                } else if (state == Player.STATE_ENDED) { // 视频播放完毕
                    Log.d(TAG, "视频播放完毕");
                }
            }
        });
        mPlayer.prepare(); // 播放器准备就绪
        mPlayer.play(); // 播放器开始播放
    }

    // 播放带字幕的视频
    private void playVideoWithSubtitle(Uri videoUri, Uri subtitleUri) {
        Log.d(TAG, "getLanguage="+Locale.getDefault().getLanguage());
        // 创建HTTP在线视频的工厂对象
        DataSource.Factory factory = new DefaultDataSource.Factory(this);
        // 创建指定地址的媒体对象
        MediaItem videoItem = new MediaItem.Builder().setUri(videoUri).build();
        // 基于工厂对象和媒体对象创建媒体来源
        MediaSource videoSource = new ProgressiveMediaSource.Factory(factory)
                .createMediaSource(videoItem);
        // 语言要填null，否则中文会乱码。selectionFlags要填Format.NO_VALUE，否则看不到字幕
        // 创建指定地址的字幕对象。ExoPlayer只支持srt字幕，不支持ass字幕
        MediaItem.SubtitleConfiguration.Builder builder = new MediaItem.SubtitleConfiguration.Builder(subtitleUri);
        builder.setMimeType(MimeTypes.APPLICATION_SUBRIP);
        builder.setSelectionFlags(Format.NO_VALUE);
        MediaItem.SubtitleConfiguration subtitleItem = builder.build();
//        MediaItem.Subtitle subtitleItem = new MediaItem.Subtitle(subtitleUri,
//                MimeTypes.APPLICATION_SUBRIP, null, Format.NO_VALUE);
        // 基于工厂对象和字幕对象创建字幕来源
        MediaSource subtitleSource = new SingleSampleMediaSource.Factory(factory)
                .createMediaSource(subtitleItem, C.TIME_UNSET);
        // 合并媒体来源与字幕来源
        MergingMediaSource mergingSource = new MergingMediaSource(videoSource, subtitleSource);
        mPlayer.setMediaSource(mergingSource); // 设置播放器的媒体来源
        mPlayer.prepare(); // 播放器准备就绪
        mPlayer.play(); // 播放器开始播放
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mPlayer.release(); // 释放播放器资源
    }

    private long mCurrentPosition = 0; // 当前的播放位置
    @Override
    public void onResume() {
        super.onResume();
        // 恢复页面时立即从上次断点开始播放视频
        if (mCurrentPosition>0 && !mPlayer.isPlaying()) {
            mPlayer.seekTo(mCurrentPosition); // 找到指定位置
        }
        mPlayer.play(); // 播放器开始播放
    }

    @Override
    public void onPause() {
        super.onPause();
        // 暂停页面时保存当前的播放进度
        if (mPlayer.isPlaying()) { // 播放器正在播放
            // 获得播放器当前的播放位置
            mCurrentPosition = mPlayer.getCurrentPosition();
            mPlayer.pause(); // 播放器暂停播放
        }
    }

    // 初始化迎宾曲下拉框
    private void initWelcomeSpinner() {
        ArrayAdapter<String> welcomeAdapter = new ArrayAdapter<>(this,
                R.layout.item_select, welcomeArray);
        Spinner sp_welcome = findViewById(R.id.sp_welcome);
        sp_welcome.setPrompt("请选择要播放的迎宾曲");
        sp_welcome.setAdapter(welcomeAdapter);
        sp_welcome.setOnItemSelectedListener(new WelcomeSelectedListener());
        sp_welcome.setSelection(3);
    }

    private String[] welcomeArray = {"首届（2018年）", "第二届（2019年）", "第三届（2020年）", "第四届（2021年）", "第五届（2022年）", "第六届（2023年）", "第七届（2024年）"};
    private String[] urlArray = {
            "https://ptgl.fujian.gov.cn:8088/masvod/public/2018/04/17/20180417_162d3639356_r38_1200k.mp4",
            "https://ptgl.fujian.gov.cn:8088/masvod/public/2019/04/15/20190415_16a1ef11c24_r38_1200k.mp4",
            "https://ptgl.fujian.gov.cn:8088/masvod/public/2020/09/26/20200926_174c8f9e4b6_r38_1200k.mp4",
            "https://ptgl.fujian.gov.cn:8088/masvod/public/2021/03/19/20210319_178498bcae9_r38.mp4",
            "https://www.fujian.gov.cn/masvod/public/2022/07/15/20220715_18201603713_r38_1200k.mp4",
            "https://www.fujian.gov.cn/masvod/public/2023/04/25/20230425_187b71018de_r38_1200k.mp4",
            "https://video.zohi.tv/fs/transcode/20240520/8cc/355193-1716184798-transv.mp4"
    };

    class WelcomeSelectedListener implements AdapterView.OnItemSelectedListener {
        public void onItemSelected(AdapterView<?> arg0, View arg1, int arg2, long arg3) {
            playVideo(Uri.parse(urlArray[arg2])); // 播放视频
        }

        public void onNothingSelected(AdapterView<?> arg0) {}
    }

}