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

static pthread_mutex_t g_lock;

static float *buffer[2]; // buffer for audio
static size_t buffer_size = 192; // actual buffer size 44100 counts in 16bit
static uint8_t currentbuffer = 0;
static size_t renderedSz[2];
static size_t sample_rate = 48000;

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

    static void libopenmpt_android_errorfunc(int error, void *userdata) {

    }

    static void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
        //LOG_D("Called! playerCallback()");
        SLresult res;
        if(isPaused) return;
        if(!isLoaded) return;

        pthread_mutex_lock(&g_lock);
        renderedSz[currentbuffer] = openmpt_module_read_interleaved_float_stereo(mod,sample_rate,buffer_size,buffer[currentbuffer]);
        if(renderedSz[currentbuffer] == 0) {
            LOG_D("Hmm. Is it end of file?");
            renderedSz[currentbuffer] = openmpt_module_read_interleaved_float_stereo(mod,sample_rate,buffer_size,buffer[currentbuffer]);
            if(renderedSz[currentbuffer] == 0) {
                isPaused = true;
                renderedSz[currentbuffer] = 1;
            }
        }
        pthread_mutex_unlock(&g_lock);

        res = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[currentbuffer],renderedSz[currentbuffer] * sizeof(float) * 2); // in byte.
        if(res != SL_RESULT_SUCCESS)
            LOG_D("Error on Enqueue? %d", res);
        currentbuffer ^= 1; // first, render.
        // this requires buffer count in sample. render will be *2 internally
    }

    static void togglePause() {
        isPaused = !isPaused;
        (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[currentbuffer],sizeof(buffer[currentbuffer]));
        currentbuffer ^= 1;
    }

    static void stopPlaying() {
        isPaused = true;
        memset(buffer[0],0,buffer_size * sizeof(float) * 2);
        memset(buffer[1],0,buffer_size * sizeof(float) * 2);
        isLoaded = false;
        if(mod != NULL) {
            openmpt_module_destroy(mod);
            mod = NULL;
        }
        isPaused = false;
    }


};

void startOpenSLES(int nsr, int fpb) {
    SLresult res;
    SLDataLocator_OutputMix loc_outMix;
    SLDataSink audioSnk;

    assert(pthread_mutex_init(&g_lock, NULL) == 0);


    buffer_size = fpb; // Stereo
    sample_rate = nsr;

    LOG_D("C-Side: OpenSL start with sample rate %d, buffersz %d",sample_rate,buffer_size);

    // allocate buffer
    buffer[0] = static_cast<float *>(malloc(buffer_size * sizeof(float) * 2)); // buffer_size is in 16bit. malloc() returns in byte. and render in stereo.
    buffer[1] = static_cast<float *>(malloc(buffer_size * sizeof(float) * 2));

    memset(buffer[0],0,buffer_size * sizeof(float) * 2);
    memset(buffer[1],0,buffer_size * sizeof(float) * 2);

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
    SLDataLocator_AndroidSimpleBufferQueue locBufQ = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 4};
    SLAndroidDataFormat_PCM_EX format_pcm_ex = {
            .formatType = SL_ANDROID_DATAFORMAT_PCM_EX,
            .numChannels = 2,
            //.sampleRate = SL_SAMPLINGRATE_44_1,
            .sampleRate = nsr==48000?SL_SAMPLINGRATE_48:SL_SAMPLINGRATE_44_1,
            //.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16,
            //.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16,
            .bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32,
            .containerSize = SL_PCMSAMPLEFORMAT_FIXED_32,
            .channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            .endianness = SL_BYTEORDER_LITTLEENDIAN,
            .representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT
            //.representation = SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT
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
    const char* errormsg;
    // TODO: Implement Error Logging Function for libopenmpt
    double b = openmpt_could_open_probability2(openmpt_stream_get_file_callbacks(),fp,1.0,
                                               &libopenmpt_android_logfunc,NULL,
                                               NULL,NULL,
                                               &err,&errormsg);
    fclose(fp);
    return b;
}


extern "C" JNIEXPORT int JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_loadFile(JNIEnv *env, jclass clazz, jstring filename) {
    const char *filename_str = env->GetStringUTFChars(filename, nullptr);
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);

    if(isLoaded) {
        isPaused = true;
        openmpt_module_destroy(mod);
        mod = NULL;
    }

    // wipe the buffer just in case
    memset(buffer[0],0,buffer_size * sizeof(float) * 2);
    memset(buffer[1],0,buffer_size * sizeof(float) * 2);

    isLoaded = false;

    FILE *fp = fopen(filename_str,"rb");

    LOG_D("Loading filename %s", filename_str);
    if (fp == NULL) {
        LOG_E("File Open Error %d (%s)", errno, strerror(errno))
        return -1;
    }
    int err;
    const char* errstr;
    mod = openmpt_module_create2(openmpt_stream_get_file_callbacks(),fp,
                                                 &libopenmpt_android_logfunc,NULL,
                                                 NULL,NULL,
                                                 &err,&errstr,NULL);

    if(mod == NULL) {
        LOG_E("Error %d %s",err,errstr);
        return -1;
    }
    isLoaded = true;
    isPaused = true;


    LOG_D("Metadata Title %s", openmpt_module_get_metadata(mod,"title"));
    // TODO: Figure out why Enqueue generates INSUFFICIENT buffer error.
    (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[currentbuffer],sizeof(buffer[currentbuffer]));
    currentbuffer ^= 1;
    fclose(fp);
    return 0;
}

extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_openOpenSLES(JNIEnv *env, jclass clazz, jint nsr, jint fpb) {

    LOG_D("OpenSL start with sample rate %d, buffersz %d",nsr,fpb);

    startOpenSLES(nsr,fpb);

}
extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_togglePause(JNIEnv *env, jclass clazz) {
    LOG_D("togglePause()");
    togglePause();
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
}

extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_stopPlaying(JNIEnv *env, jclass clazz)
{
    LOG_D("stopPlaying()");
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
    stopPlaying();
}

extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_closeOpenSLES(JNIEnv *env, jclass clazz) {
    LOG_D("OpenSL close");
    endOpenSLES();
}

extern "C" JNIEXPORT jint JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getNumChannel(JNIEnv *env, jclass clazz) {
    if(mod == NULL) return 0;
    if(!isLoaded) return 0;
    pthread_mutex_lock(&g_lock);
    int r = openmpt_module_get_num_channels(mod);
    pthread_mutex_unlock(&g_lock);
    return r;
}

extern "C" JNIEXPORT jfloat JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getVULeft(JNIEnv *env, jclass clazz, jint nums) {
    if(mod == NULL) return 0.0f;
    if(!isLoaded) return 0.0f;
    pthread_mutex_lock(&g_lock);
    float r = openmpt_module_get_current_channel_vu_left(mod,nums);
    pthread_mutex_unlock(&g_lock);
    return r;

}

extern "C" JNIEXPORT jfloat JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getVURight(JNIEnv *env, jclass clazz, jint nums) {
    if(mod == NULL) return 0.0f;
    if(!isLoaded) return 0.0f;
    pthread_mutex_lock(&g_lock);
    float r = openmpt_module_get_current_channel_vu_right(mod,nums);
    pthread_mutex_unlock(&g_lock);
    return r;
}
extern "C"
JNIEXPORT jint JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getOrder(JNIEnv *env, jclass clazz) {
    if(mod == NULL) return 0;
    if(!isLoaded) return 0;
    pthread_mutex_lock(&g_lock);
    int r = openmpt_module_get_current_order(mod);
    pthread_mutex_unlock(&g_lock);
    return r;

}
extern "C"
JNIEXPORT jint JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getPattern(JNIEnv *env, jclass clazz) {
    if(mod == NULL) return 0;
    if(!isLoaded) return 0;
    pthread_mutex_lock(&g_lock);
    int r = openmpt_module_get_current_pattern(mod);
    pthread_mutex_unlock(&g_lock);
    return r;
}
extern "C"
JNIEXPORT jint JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getRow(JNIEnv *env, jclass clazz) {
    if(mod == NULL) return 0;
    if(!isLoaded) return 0;
    pthread_mutex_lock(&g_lock);
    int r = openmpt_module_get_current_row(mod);
    pthread_mutex_unlock(&g_lock);
    return r;
}
extern "C"
JNIEXPORT jint JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getSpeed(JNIEnv *env, jclass clazz) {
    if(mod == NULL) return 0;
    if(!isLoaded) return 0;
    pthread_mutex_lock(&g_lock);
    int r = openmpt_module_get_current_speed(mod);
    pthread_mutex_unlock(&g_lock);
    return r;
}
extern "C"
JNIEXPORT jint JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getTempo(JNIEnv *env, jclass clazz) {
    if(mod == NULL) return 0;
    if(!isLoaded) return 0;
    pthread_mutex_lock(&g_lock);
    int r = openmpt_module_get_current_tempo(mod);
    pthread_mutex_unlock(&g_lock);
    return r;
}

extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_setRenderParam(JNIEnv *env, jclass clazz, jint param, jint value) {
    if(mod == NULL) return;
    if(!isLoaded) return;
    pthread_mutex_lock(&g_lock);
    openmpt_module_set_render_param(mod,param,value);
    pthread_mutex_unlock(&g_lock);
}
extern "C" JNIEXPORT void JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_ctlSetRepeat(JNIEnv *env, jclass clazz, jint repeat_count) {
    if(mod == NULL) return;
    if(!isLoaded) return;
    pthread_mutex_lock(&g_lock);
    openmpt_module_set_repeat_count(mod,repeat_count);
    pthread_mutex_unlock(&g_lock);
}

extern "C" JNIEXPORT jstring JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getMetadata(JNIEnv *env, jclass clazz, jstring key) {
    const char *key_str = env->GetStringUTFChars(key, nullptr);
    jstring ret = env->NewStringUTF("");
    if(mod == NULL) return ret;
    if(!isLoaded) return ret;
    pthread_mutex_lock(&g_lock);
    const char* meta = openmpt_module_get_metadata(mod,key_str);
    pthread_mutex_unlock(&g_lock);
    ret = env->NewStringUTF(meta);
    return ret;
}
extern "C" JNIEXPORT jstring JNICALL Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_rRowStrings(JNIEnv *env, jclass clazz) {
    static char string_buffer[256]; // "Spd:%02d BPM:%3d Pos:%02X Ptn:%02X Ord:%3d"

    pthread_mutex_lock(&g_lock);
    int speed = openmpt_module_get_current_speed(mod);
    int bpm = openmpt_module_get_current_tempo(mod);
    int pos = openmpt_module_get_current_row(mod);
    int ptn = openmpt_module_get_current_pattern(mod);
    int ord = openmpt_module_get_current_order(mod);
    pthread_mutex_unlock(&g_lock);

    snprintf(string_buffer,128,"Spd:%02d BPM:%3d Pos:%02X Ptn:%02X Ord:%3d",
             speed,bpm,pos,ptn,ord);
    return env->NewStringUTF(string_buffer);
}
extern "C"
JNIEXPORT jstring JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_rVUStrings(JNIEnv *env, jclass clazz, jint num) {
    static char string_buffer[256]; // "%02d: L:%f R:%f "


    pthread_mutex_lock(&g_lock);
    int pos = openmpt_module_get_current_row(mod);
    int ptn = openmpt_module_get_current_pattern(mod);
    int ord = openmpt_module_get_current_order(mod);

    const char* fm = openmpt_module_format_pattern_row_channel(mod,ptn,pos,num,0,false);

    float l = openmpt_module_get_current_channel_vu_left(mod,num);
    float r = openmpt_module_get_current_channel_vu_right(mod,num);

    pthread_mutex_unlock(&g_lock);

    snprintf(string_buffer,128,"%02d: L:%f R:%f %s"
             ,num,l,r
             ,fm
             );
    return env->NewStringUTF(string_buffer);
}
extern "C"
JNIEXPORT jdouble JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getCurrentTime(JNIEnv *env, jclass clazz) {
    pthread_mutex_lock(&g_lock);
    double elapsed = openmpt_module_get_position_seconds(mod);
    pthread_mutex_unlock(&g_lock);
    return elapsed;
}
extern "C"
JNIEXPORT jdouble JNICALL
Java_team_digitalfairy_lencel_libopenmpt_1jni_1test_LibOpenMPT_getModuleTime(JNIEnv *env, jclass clazz) {
    pthread_mutex_lock(&g_lock);
    double t = openmpt_module_get_duration_seconds(mod);
    pthread_mutex_unlock(&g_lock);
    return t;
}