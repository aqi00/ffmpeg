#include "opensl.h"

// 是否成功
bool OpenslHelper::isSuccess(SLresult &result) {
    if (result == SL_RESULT_SUCCESS) {
        return true;
    }
    return false;
}

// 创建OpenSL引擎与引擎接口
SLresult OpenslHelper::createEngine() {
    // 创建引擎
    result = slCreateEngine(&engine, 0, NULL, 0, NULL, NULL);
    if (!isSuccess(result)) {
        return result;
    }

    // 实例化引擎，第二个参数为：是否异步
    result = (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取引擎接口
    result = (*engine)->GetInterface(engine, SL_IID_ENGINE, &engineItf);
    if (!isSuccess(result)) {
        return result;
    }

    return result;
}

// 创建混音器与混音接口
SLresult OpenslHelper::createMix() {
    // 获取混音器
    result = (*engineItf)->CreateOutputMix(engineItf, &mix, 0, 0, 0);
    if (!isSuccess(result)) {
        return result;
    }

    // 实例化混音器
    result = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取环境混响混音器接口
    SLresult envResult = (*mix)->GetInterface(
            mix, SL_IID_ENVIRONMENTALREVERB, &envItf);
    if (isSuccess(envResult)) {
        // 给混音器设置环境
        (*envItf)->SetEnvironmentalReverbProperties(envItf, &settings);
    }

    return result;
}

// 创建播放器与播放接口。输入参数包括：声道数，采样率，采样位数（量化格式），立体声掩码
SLresult OpenslHelper::createPlayer(int numChannels, long samplesRate, int bitsPerSample, int channelMask) {
    // 关联音频流缓冲区。设为2是防止延迟，可以在播放另一个缓冲区时填充新数据
    SLDataLocator_AndroidSimpleBufferQueue buffQueque = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

    // 缓冲区格式
//    typedef struct SLDataFormat_PCM_ {
//        SLuint32 		formatType;    //格式pcm
//        SLuint32 		numChannels;   //声道数
//        SLuint32 		samplesPerSec; //采样率
//        SLuint32 		bitsPerSample; //采样位数（量化格式）
//        SLuint32 		containerSize; //包含位数
//        SLuint32 		channelMask;   //立体声
//        SLuint32		endianness;    //结束标志位
//    } SLDataFormat_PCM;
    SLDataFormat_PCM dataFormat_pcm = {
            SL_DATAFORMAT_PCM, (SLuint32) numChannels, (SLuint32) samplesRate,
            (SLuint32) bitsPerSample, (SLuint32) bitsPerSample,
            (SLuint32) channelMask, SL_BYTEORDER_LITTLEENDIAN};

    // 存放缓冲区地址和格式地址的结构体
//    typedef struct SLDataSource_ {
//        void *pLocator;//缓冲区
//        void *pFormat;//格式
//    } SLDataSource;
    SLDataSource audioSrc = {&buffQueque, &dataFormat_pcm};

    // 关联混音器
//    typedef struct SLDataLocator_OutputMix {
//        SLuint32 		locatorType;
//        SLObjectItf		outputMix;
//    } SLDataLocator_OutputMix;
    SLDataLocator_OutputMix dataLocator_outputMix = {SL_DATALOCATOR_OUTPUTMIX, mix};

    // 混音器快捷方式
//    typedef struct SLDataSink_ {
//        void *pLocator;
//        void *pFormat;
//    } SLDataSink;
    SLDataSink audioSink = {&dataLocator_outputMix, NULL};

    // 通过引擎接口创建播放器
//    SLresult (*CreateAudioPlayer) (
//            SLEngineItf self,
//            SLObjectItf * pPlayer,
//            SLDataSource *pAudioSrc,
//            SLDataSink *pAudioSnk,
//            SLuint32 numInterfaces,
//            const SLInterfaceID * pInterfaceIds,
//            const SLboolean * pInterfaceRequired
//    );
    SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    SLboolean required[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    // 创建音频播放器
    result = (*engineItf)->CreateAudioPlayer(
            engineItf, &player, &audioSrc, &audioSink, 3, ids, required);
    if (!isSuccess(result)) {
        return result;
    }

    // 实例化播放器
    result = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取播放接口
    result = (*player)->GetInterface(player, SL_IID_PLAY, &playItf);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取音量接口
    result = (*player)->GetInterface(player, SL_IID_VOLUME, &volumeItf);
    if (!isSuccess(result)) {
        return result;
    }

    // 注册缓冲区
    result = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &bufferQueueItf);
    if (!isSuccess(result)) {
        return result;
    }

    return result;
}

// 注册回调入口
SLresult OpenslHelper::registerCallback(slAndroidSimpleBufferQueueCallback callback) {
    // 注册回调入口
    result = (*bufferQueueItf)->RegisterCallback(bufferQueueItf, callback, NULL);
    return result;
}

// 开始播放
SLresult OpenslHelper::play() {
    playState = SL_PLAYSTATE_PLAYING;
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
    return result;
}

// 暂停播放
SLresult OpenslHelper::pause() {
    playState = SL_PLAYSTATE_PAUSED;
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PAUSED);
    return result;
}

// 停止播放
SLresult OpenslHelper::stop() {
    playState = SL_PLAYSTATE_STOPPED;
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
    return result;
}

// 析构
OpenslHelper::~OpenslHelper() {
    if (player != NULL) {
        (*player)->Destroy(player);
        player = NULL; // 播放器置空
        playItf = NULL; // 播放接口置空
        bufferQueueItf = NULL; // 缓冲区置空
        volumeItf = NULL; // 音量置空
    }
    if (mix != NULL) {
        (*mix)->Destroy(mix);
        mix = NULL; // 混音器置空
        envItf = NULL; // 环境混响混音器接口置空
    }
    if (engine != NULL) {
        (*engine)->Destroy(engine);
        engine = NULL; // OpenSL引擎置空
        engineItf = NULL; // OpenSL引擎接口置空
    }
    result = (SLresult) NULL;
}

