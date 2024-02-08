package com.example.audio.util;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.util.Log;

public class FFmpegUtil {
    private final static String TAG = "FFmpegUtil";
    private AudioTrack mAudioTrack; // 播放PCM音频的音轨

    static {
        System.loadLibrary("ffmpeg"); // 加载动态库libffmpeg.so
    }

    public static native void playAudioByTrack(String audioPath);

    public static native void stopPlayByTrack();

    public static native void playAudioByOpenSL(String audioPath);

    public static native void stopPlayByOpenSL();

    // 创建音轨对象。给jni层回调
    private void create(int sampleRate, int channelCount) {
        Log.d(TAG, "create sampleRate="+sampleRate+", channelCount="+channelCount);
        int channelType = (channelCount==1) ? AudioFormat.CHANNEL_OUT_MONO
                : AudioFormat.CHANNEL_OUT_STEREO;
        // 根据定义好的几个配置，来获取合适的缓冲大小
        int bufferSize = AudioTrack.getMinBufferSize(sampleRate,
                channelType, AudioFormat.ENCODING_PCM_16BIT);
        Log.d(TAG, "bufferSize="+bufferSize);
        // 根据音频配置和缓冲区构建原始音频播放实例
        mAudioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
                sampleRate, channelType, AudioFormat.ENCODING_PCM_16BIT,
                bufferSize, AudioTrack.MODE_STREAM);
        mAudioTrack.play(); // 开始播放原始音频
        Log.d(TAG, "end create");
    }

    // 播放PCM音频。给jni层回调
    private void play(byte[] bytes, int size) {
        if (mAudioTrack != null &&
                mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
            mAudioTrack.write(bytes, 0, size); // 将数据写入到音轨AudioTrack
        }
    }

}
