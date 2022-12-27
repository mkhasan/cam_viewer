//
// Created by usrc on 16. 12. 12.
//

#ifndef MEDIAPLAYER_PLAYER_H
#define MEDIAPLAYER_PLAYER_H

#include <jni.h>
#include <android/native_window.h>







#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>




struct Player * get_player(JNIEnv *env, jobject thiz);
int jni_player_init(JNIEnv *env, jobject thiz);
int jni_player_set_data_source(JNIEnv *env, jobject thiz, const char *file_path);
int player_start_decoding_threads(struct Player *player);
int64_t player_get_current_video_time(struct Player *player);
void jni_player_resume(JNIEnv *env, jobject thiz);
void jni_player_resume(JNIEnv *env, jobject thiz);
void jni_player_dealloc(JNIEnv *env, jobject thiz);
void jni_player_stop(JNIEnv *env, jobject thiz);
void jni_player_render(JNIEnv *env, jobject thiz, jobject surface);
void jni_player_render_frame_stop(JNIEnv *env, jobject thiz);
void callback(struct Player * player, JNIEnv *env);

#ifdef __cplusplus
}
#endif



#define LOG_LEVEL 10
#define LOG_TAG "pi-dr-native-player"
#define MAX_STREAMS 3
#define MAX_STRLEN 100


#define LOG(...) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#define LOGE(level, ...) if (level <= LOG_LEVEL + 10) {__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);}
#define LOGW(level, ...) if (level <= LOG_LEVEL + 5) {__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__);}



enum PlayerErrors {
    ERROR_NO_ERROR = 0,

    // Java errors
            ERROR_COULD_NOT_CREATE_GLOBAL_REF,
    ERROR_NOT_FOUND_PLAYER_CLASS,
    ERROR_NOT_FOUND_PREPARE_FRAME_METHOD,
    ERROR_NOT_FOUND_ON_UPDATE_TIME_METHOD,
    ERROR_NOT_FOUND_PREPARE_AUDIO_TRACK_METHOD,
    ERROR_NOT_FOUND_SET_STREAM_INFO_METHOD,
    ERROR_NOT_FOUND_M_NATIVE_PLAYER_FIELD,
    ERROR_COULD_NOT_GET_JAVA_VM,
    ERROR_COULD_NOT_DETACH_THREAD,
    ERROR_COULD_NOT_ATTACH_THREAD,
    ERROR_COULD_NOT_CREATE_GLOBAL_REF_FOR_AUDIO_TRACK_CLASS,

    // AudioTrack
            ERROR_NOT_FOUND_AUDIO_TRACK_CLASS,
    ERROR_NOT_FOUND_WRITE_METHOD,
    ERROR_NOT_FOUND_PLAY_METHOD,
    ERROR_NOT_FOUND_PAUSE_METHOD,
    ERROR_NOT_FOUND_STOP_METHOD,
    ERROR_NOT_FOUND_GET_CHANNEL_COUNT_METHOD,
    ERROR_NOT_FOUND_FLUSH_METHOD,
    ERROR_NOT_FOUND_GET_SAMPLE_RATE_METHOD,

    ERROR_COULD_NOT_CREATE_AVCONTEXT,
    ERROR_COULD_NOT_OPEN_VIDEO_FILE,
    ERROR_COULD_NOT_OPEN_STREAM,
    ERROR_COULD_NOT_OPEN_VIDEO_STREAM,
    ERROR_COULD_NOT_FIND_VIDEO_CODEC,
    ERROR_COULD_NOT_OPEN_VIDEO_CODEC,
    ERROR_COULD_NOT_ALLOC_FRAME,

    ERROR_NOT_CREATED_BITMAP,
    ERROR_COULD_NOT_GET_SWS_CONTEXT,
    ERROR_COULD_NOT_PREPARE_PACKETS_QUEUE,
    ERROR_COULD_NOT_FIND_AUDIO_STREAM,
    ERROR_COULD_NOT_FIND_AUDIO_CODEC,
    ERROR_COULD_NOT_OPEN_AUDIO_CODEC,
    ERROR_COULD_NOT_PREPARE_RGB_QUEUE,
#ifdef SUBTITLES
    ERROR_COULD_NOT_PREPARE_SUBTITLES_QUEUE,
	ERROR_COULD_NOT_INIT_ASS_LIBRARY,
	ERROR_COULD_NOT_PREAPARE_ASS_TRACK,
	ERROR_COULD_NOT_PREPARE_ASS_RENDERER,
#endif // SUBTITLES
    ERROR_COULD_NOT_PREPARE_AUDIO_PACKETS_QUEUE,
    ERROR_COULD_NOT_PREPARE_VIDEO_PACKETS_QUEUE,

    ERROR_WHILE_DUPLICATING_FRAME,

    ERROR_WHILE_DECODING_VIDEO,
    ERROR_COULD_NOT_RESAMPLE_FRAME,
    ERROR_WHILE_ALLOCATING_AUDIO_SAMPLE,
    ERROR_WHILE_DECODING_AUDIO_FRAME,
    ERROR_NOT_CREATED_AUDIO_TRACK,
    ERROR_NOT_CREATED_AUDIO_TRACK_GLOBAL_REFERENCE,
    ERROR_COULD_NOT_INIT_SWR_CONTEXT,
    ERROR_NOT_CREATED_AUDIO_SAMPLE_BYTE_ARRAY,
    ERROR_PLAYING_AUDIO,
    ERROR_WHILE_LOCING_BITMAP,

    ERROR_COULD_NOT_JOIN_PTHREAD,
    ERROR_COULD_NOT_INIT_PTHREAD_ATTR,
    ERROR_COULD_NOT_CREATE_PTHREAD,
    ERROR_COULD_NOT_DESTROY_PTHREAD_ATTR,
    ERROR_COULD_NOT_ALLOCATE_MEMORY,
    ERROR_PTS_IS_ZERO,
    MAX_STRLEN_LIMIT_EXCEEDED,
    ON_UPDATE_TIME_METHOD_NOT_FOUND
};

#define MIN_SLEEP_TIME_US 1000ll

#define AV_LOG_QUIET    -8
#define AV_LOG_PANIC     0
#define AV_LOG_FATAL     8
#define AV_LOG_ERROR    16
#define AV_LOG_WARNING  24
#define AV_LOG_INFO     32
#define AV_LOG_VERBOSE  40
#define AV_LOG_DEBUG    48

#define TRUE 1
#define FALSE 0


enum StreamNumber {
    NO_STREAM = -2,
    UNKNOWN_STREAM = -1,
};

struct Player {

    JavaVM *get_javavm;
    jobject thiz;

    ANativeWindow * window;

    AVFrame *rgb_frame;

    AVFrame *tmp_frame;
    uint8_t *tmp_buffer;
    AVFrame *tmp_frame2;
    uint8_t *tmp_buffer2;

    int64_t video_duration;
    int64_t last_updated_time;

    pthread_mutex_t mutex_interrupt;

    AVIOInterruptCB interrupt_callback;

    int interrupt;

    pthread_mutex_t mutex_operation;

    int capture_streams_no;

    int video_stream_no;
    int audio_stream_no;
    int no_audio;

    AVStream *input_streams[MAX_STREAMS];
    AVCodecContext * input_codec_ctxs[MAX_STREAMS];
    int input_stream_numbers[MAX_STREAMS];
    AVFrame *input_frames[MAX_STREAMS];

    AVFormatContext *input_format_ctx;
    int input_inited;

    struct SwsContext *sws_context;

    struct SwrContext *swr_context;

    int playing;

    pthread_mutex_t mutex_queue;
    pthread_cond_t cond_queue;
    Queue *packets[MAX_STREAMS];

    int pause;
    int stop;

    int flush_streams[MAX_STREAMS];
    int flush_video_play;

    int stop_streams[MAX_STREAMS];

    pthread_t thread_player_read_from_stream;
    pthread_t decode_threads[MAX_STREAMS];

    int thread_player_read_from_stream_created;
    int decode_threads_created[MAX_STREAMS];


    int64_t start_time;
    int64_t pause_time;


    int Test;

    int error;

    char file_path[MAX_STRLEN];
    //AVCodecContext    *pCodecCtx;

};

struct State {
    struct Player *player;
    JNIEnv* env;
};

struct PacketData {
    int end_of_stream;
    AVPacket *packet;
};

struct DecoderState {
    int stream_no;
    struct Player *player;
    JNIEnv* env;
};

struct DecoderData {
    struct Player *player;
    int stream_no;
};



static const char player_m_native_player[2][MAX_STRLEN] = {"mNativePlayer", "J"};



#endif //MEDIAPLAYER_PLAYER_H
