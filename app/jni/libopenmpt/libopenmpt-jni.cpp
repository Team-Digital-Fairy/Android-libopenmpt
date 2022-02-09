#include <jni.h>
#include <istream>
#include <cstdio>
#include <iostream>
#include <fstream>

#include <pthread.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <libopenmpt/libopenmpt.hpp>

#include <android/log.h>

#define LOG_TAG "JNI-LibOpenMPT"

#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,  __VA_ARGS__);
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,  __VA_ARGS__);

static uint8_t *buffer[2]; // buffer for audio

static SLObjectItf engineObject;
static SLEngineItf engineEngine;
static SLObjectItf outputMixObject;

static SLObjectItf  bqPlayerObject = NULL;
static SLPlayItf    bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf    bqPlayerBufferQueue;
static SLVolumeItf                      bqPlayerVolume;

static bool isPaused = true;

extern "C" {

    static void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
        // do nothing atm.
    }
};

void startOpenSLES() {
    SLresult res;
    SLDataLocator_OutputMix loc_outMix;
    SLDataSink audioSnk;

    res = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(res == SL_RESULT_SUCCESS);
    res = (*engineObject)->Realize(engineObject,SL_BOOLEAN_FALSE);
    assert(res == SL_RESULT_SUCCESS);
    res = (*engineObject)->GetInterface(engineObject,SL_IID_ENGINE,&engineEngine);
    assert(res == SL_RESULT_SUCCESS);
    res = (*engineEngine)->CreateOutputMix(engineEngine,&outputMixObject,0,NULL,NULL);
    assert(res == SL_RESULT_SUCCESS);
    res = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(res == SL_RESULT_SUCCESS);

    //SLDataFormat_PCM format_pcm;
    SLDataLocator_AndroidSimpleBufferQueue locBufQ = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLAndroidDataFormat_PCM_EX format_pcm_ex = {
            .formatType = SL_ANDROID_DATAFORMAT_PCM_EX,
            .numChannels = 2,
            .sampleRate = SL_SAMPLINGRATE_48,
            .bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32,
            .containerSize = SL_PCMSAMPLEFORMAT_FIXED_32,
            .channelMask = SL_BYTEORDER_LITTLEENDIAN,
            .representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT
    };
    SLDataSource audioSrc = {
            .pLocator = &locBufQ,
            .pFormat = &format_pcm_ex
    };

    loc_outMix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    loc_outMix.outputMix = outputMixObject;
    audioSnk.pLocator = &loc_outMix;
    audioSnk.pFormat = NULL;

    const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    res = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 2, ids, req);
    assert(res == SL_RESULT_SUCCESS);
    res = (*bqPlayerObject)->Realize(bqPlayerObject,SL_BOOLEAN_FALSE);
    assert(res == SL_RESULT_SUCCESS);
    res = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(res == SL_RESULT_SUCCESS);
    res = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,&bqPlayerBufferQueue);
    assert(res == SL_RESULT_SUCCESS);

    // This is where you set callback.
    res = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, playerCallback, NULL);
    assert(res == SL_RESULT_SUCCESS);
    res = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(res == SL_RESULT_SUCCESS);
    res = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(res == SL_RESULT_SUCCESS);

    res = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[0],sizeof(buffer[0]));

    if(res != SL_RESULT_SUCCESS)
}



extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_HelloWorld(JNIEnv *env, jclass thiz) {
    // TODO: implement HelloWorld()
    LOG_D("aaaaaaaa it worksssss!");
    LOG_D("%s", openmpt::string::get("library_version").c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getOpenMPTString(JNIEnv *env, jclass thiz, jstring key) {
    const char *keyStr = env->GetStringUTFChars(key, nullptr);
    return env->NewStringUTF(openmpt::string::get(keyStr).c_str());
}


extern "C" JNIEXPORT jdouble JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_OpenProbability(JNIEnv *env, jclass thiz, jstring filename, jdouble effort) {
    //FILE *fp;
    const char *filename_str = env->GetStringUTFChars(filename, nullptr);
    std::ifstream file_stream(filename_str, std::ios::in | std::ios::binary);

    LOG_D("Loading filename %s", filename_str);

    if (!file_stream) {
        LOG_E("File Open Error %d (%s)", errno, strerror(errno))
        return 0.0;
    }

    double b = openmpt::could_open_probability(file_stream, 1.0);
    return b;
}


extern "C" JNIEXPORT int JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_loadFile(JNIEnv *env, jclass clazz, jstring filename) {
    const char *filename_str = env->GetStringUTFChars(filename, nullptr);
    std::ifstream file_stream(filename_str, std::ios::in | std::ios::binary);

    LOG_D("Loading filename %s", filename_str);
    if (!file_stream) {
        LOG_E("File Open Error %d (%s)", errno, strerror(errno))
        return -1;
    }

    openmpt::module mod(file_stream);
    LOG_D("Metadata Title %s", mod.get_metadata("message").c_str())

    return 0;
}

extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_OpenSLES_1Test(JNIEnv *env, jclass clazz) {

    LOG_D("OpenSL ES Testing...");

}