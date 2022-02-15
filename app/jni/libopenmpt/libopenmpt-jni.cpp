#include <jni.h>
#include <istream>
#include <cstdio>
#include <iostream>
#include <fstream>

#include <pthread.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>

#include <android/log.h>

#define LOG_TAG "JNI-LibOpenMPT"

#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,  __VA_ARGS__);
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,  __VA_ARGS__);

static float *buffer[2]; // buffer for audio
static uint8_t currentbuffer = 0;

static SLObjectItf engineObject;
static SLEngineItf engineEngine;
static SLObjectItf outputMixObject;

static SLObjectItf  bqPlayerObject = NULL;
static SLPlayItf    bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf    bqPlayerBufferQueue;
static SLVolumeItf                      bqPlayerVolume;

static bool isPaused = true;
static bool isLoaded = false;

openmpt_module *mod = NULL;

extern "C" {
    static void libopenmpt_android_logfunc(const char* message, void *userdata) {
        if(message) LOG_D("%s",message);
    }


    static void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
        LOG_D("Called! playerCallback()");
        SLresult res;
        if(isPaused) return;
        if(!isLoaded) return;

        openmpt_module_read_interleaved_float_stereo(mod,48000,48000,buffer[currentbuffer]);
        res = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[currentbuffer],48000);
        if(res != SL_RESULT_SUCCESS)
            LOG_D("%d", res);
        currentbuffer ^= 1;
    }

    static void togglePause() {
        isPaused = false;
    }


};

void startOpenSLES() {
    SLresult res;
    SLDataLocator_OutputMix loc_outMix;
    SLDataSink audioSnk;

    // allocate buffer
    buffer[0] = static_cast<float *>(malloc(48000 * 2));
    buffer[1] = static_cast<float *>(malloc(48000 * 2));

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
            .channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            .endianness = SL_BYTEORDER_LITTLEENDIAN,
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
    res = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);
    assert(res == SL_RESULT_SUCCESS);

    res = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[currentbuffer],sizeof(buffer[currentbuffer]));

    if(res != SL_RESULT_SUCCESS) {
        abort();
    }

    currentbuffer ^= 1;
}

void endOpenSLES() {
    SLresult res;
    res = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
    assert(res == SL_RESULT_SUCCESS);

    if (bqPlayerObject != NULL)
    {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = NULL;
        bqPlayerPlay = NULL;
        bqPlayerBufferQueue = NULL;
        bqPlayerVolume = NULL;
    }

    if (outputMixObject != NULL)
    {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
    }

    if (engineObject != NULL)
    {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
    free(buffer[0]);
    free(buffer[1]);
}


extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_HelloWorld(JNIEnv *env, jclass thiz) {
    // TODO: implement HelloWorld()
    LOG_D("aaaaaaaa it worksssss!");
    LOG_D("%s", openmpt_get_string("library_version"));
    //LOG_D("%s", openmpt::string::get("library_version").c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getOpenMPTString(JNIEnv *env, jclass thiz, jstring key) {
    const char *keyStr = env->GetStringUTFChars(key, nullptr);
    const char *str = openmpt_get_string(keyStr);
    jstring jsr = env->NewStringUTF(str);
    openmpt_free_string(str);
    return jsr;
}


extern "C" JNIEXPORT jdouble JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_OpenProbability(JNIEnv *env, jclass thiz, jstring filename, jdouble effort) {
    //FILE *fp;
    const char *filename_str = env->GetStringUTFChars(filename, nullptr);
    //std::ifstream file_stream(filename_str, std::ios::in | std::ios::binary);
    LOG_D("Loading filename %s", filename_str);
    FILE *fp = fopen(filename_str,"rb");
    if (fp == NULL) {
        LOG_E("File Open Error %d (%s)", errno, strerror(errno))
        return 0.0;
    }
    int err;
    // TODO: Implement Error Logging Function for libopenmpt
    double b = openmpt_could_open_probability2(openmpt_stream_get_file_callbacks(),fp,1.0,
                                               &libopenmpt_android_logfunc,NULL,
                                               NULL,NULL,
                                               &err,NULL);
    fclose(fp);
    return b;
}


extern "C" JNIEXPORT int JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_loadFile(JNIEnv *env, jclass clazz, jstring filename) {
    const char *filename_str = env->GetStringUTFChars(filename, nullptr);
    if(isLoaded) {
        isPaused = true;
        openmpt_module_destroy(mod);
    }
    isLoaded = false;

    FILE *fp = fopen(filename_str,"rb");

    LOG_D("Loading filename %s", filename_str);
    if (fp == NULL) {
        LOG_E("File Open Error %d (%s)", errno, strerror(errno))
        return -1;
    }
    int err;
    mod = openmpt_module_create2(openmpt_stream_get_file_callbacks(),fp,
                                                 &libopenmpt_android_logfunc,NULL,
                                                 NULL,NULL,
                                                 &err,NULL,NULL);


    isLoaded = true;
    isPaused = true;

    //openmpt::modulemod(file_stream);
    LOG_D("Metadata Title %s", openmpt_module_get_metadata(mod,"title"));
    //fclose(fp);
    return 0;
}

extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_openOpenSLES(JNIEnv *env, jclass clazz) {

    LOG_D("OpenSL start");
    startOpenSLES();

}
extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_togglePause(JNIEnv *env, jclass clazz) {
    LOG_D("togglePause()");
    togglePause();
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
}

extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_closeOpenSLES(JNIEnv *env, jclass clazz) {
    LOG_D("OpenSL close");
    endOpenSLES();
}