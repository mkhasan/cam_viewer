#include <jni.h>


#include <android/bitmap.h>
#include <android/log.h>


#include <android/native_window.h>
#include <android/native_window_jni.h>





#include <string>
#include <stdlib.h>
#include "native_test.h"



#include "player.h"



struct Player * GetPlayer(        JNIEnv *env,
                                  jobject thisObj) {

    return get_player(env, thisObj);
}




extern "C"
jint
Java_com_railbot_usrc_mediaplayer_VideoPlayer_initNative(
        JNIEnv *env,
        jobject thisObj/* this */) {


/*

    jclass thisClass = env->GetObjectClass(thisObj);
    jmethodID midCallBack = env->GetMethodID(env, player->thiz, "callback", "()V");

    if (NULL == midCallBack) {
        LOGE(0, "callback error");
        return;
    }
    (*env)->CallVoidMethod(env, player->thiz, midCallBack);

    */
    LOGE(1, "Init player");
    return jni_player_init(env, thisObj);



}

extern "C"
void
Java_com_railbot_usrc_mediaplayer_VideoPlayer_deallocNative(
        JNIEnv *env,
        jobject thisObj) {


    LOGE(1, "deallocating");
    jni_player_dealloc(env, thisObj);


}

extern "C"
void
Java_com_railbot_usrc_mediaplayer_VideoPlayer_stopNative(
        JNIEnv *env,
        jobject thisObj) {
    std::string hello = "Hello from stop";

    LOGE(1, "Stopping...");
    jni_player_stop(env, thisObj);




}

extern "C"
void
Java_com_railbot_usrc_mediaplayer_VideoPlayer_render(
        JNIEnv *env,
        jobject thisObj, jobject surface) {

    jni_player_render(env, thisObj, surface);
    LOGE(1, "rendering");

}

extern "C"
void
Java_com_railbot_usrc_mediaplayer_VideoPlayer_Test(
        JNIEnv *env,
        jobject thisObj) {

    jni_player_resume(env, thisObj);
    LOGE(1, "Testing");
    //thread_func((void *) player);

}

extern "C"
void
Java_com_railbot_usrc_mediaplayer_VideoPlayer_renderFrameStart(
        JNIEnv *env,
        jobject thisObj) {


    LOGE(1, "renderFrameString");
    //thread_func((void *) player);

}

extern "C"
void
Java_com_railbot_usrc_mediaplayer_VideoPlayer_renderFrameStop(
        JNIEnv *env,
        jobject thisObj) {

    LOGE(1, "renderFrameStopping");
    jni_player_render_frame_stop(env, thisObj);

    //thread_func((void *) player);

}


extern "C"
jint
Java_com_railbot_usrc_mediaplayer_VideoPlayer_setDataSourceNative(
        JNIEnv *env,
        jobject thisObj, jstring _url) {


    const char *url = env->GetStringUTFChars(_url, NULL);

    int ret =  jni_player_set_data_source(env, thisObj, url);

    env->ReleaseStringUTFChars(_url, url);


    if (ret != 0)
        LOGE(1, "error !!! \n");

    if(ret == 0)
        jni_player_resume(env, thisObj);

    return (jint) ret;

}


