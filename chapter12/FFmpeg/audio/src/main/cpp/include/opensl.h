#ifndef FFMPEG_AUDIO_OPENSL_H
#define FFMPEG_AUDIO_OPENSL_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class OpenslHelper {
public:
    // 返回结果
    SLresult result;
    // OpenSL引擎
    SLObjectItf engine;
    // OpenSL引擎接口
    SLEngineItf engineItf;
    // 混音器
    SLObjectItf mix;
    // 环境混响混音器接口
    SLEnvironmentalReverbItf envItf;
    // 环境混响混音器环境
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    // 播放器
    SLObjectItf player;
    // 播放接口
    SLPlayItf playItf;
    // 缓冲区
    SLAndroidSimpleBufferQueueItf bufferQueueItf;
    // 音量接口
    SLVolumeItf volumeItf;
    // 播放状态 SL_PLAYSTATE_XXX
    SLuint32 playState;

    // 创建OpenSL引擎与引擎接口
    SLresult createEngine();

    // 是否成功
    bool isSuccess(SLresult &result);

    // 创建混音器与混音接口
    SLresult createMix();

    // 创建播放器与播放接口。输入参数包括：声道数，采样率，采样位数（量化格式），立体声掩码
    SLresult createPlayer(int numChannels, long samplesRate, int bitsPerSample, int channelMask);

    // 开始播放
    SLresult play();

    // 暂停播放
    SLresult pause();

    // 停止播放
    SLresult stop();

    // 注册回调入口。播放器会不断调用该函数，需要在此回调中持续向缓冲区填充数据
    SLresult registerCallback(slAndroidSimpleBufferQueueCallback callback);

    ~OpenslHelper();
};

#endif //FFMPEG_AUDIO_OPENSL_H
