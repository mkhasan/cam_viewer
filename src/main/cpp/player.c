//
// Created by usrc on 16. 12. 13.
//

#include <jni.h>

#include <android/log.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include <android/native_window_jni.h>


#include "player.h"


void player_create_context_free(struct Player *player);
int player_ctx_interrupt_callback(void *p);
int player_open_input(struct Player *player, const char *file_path);
int player_find_stream(struct Player *player, enum AVMediaType codec_type, int recommended_stream_no);
int player_alloc_video_frames(struct Player *player);
int player_alloc_frames(struct Player *player);
int player_alloc_queues(struct State *state);
void *player_fill_packet(struct State *state);
void player_free_packet(struct State *player, struct PacketData *elem);
void player_get_video_duration(struct Player *player);



int player_set_data_source(struct State *state, const char *file_path);
void player_stop_without_lock(struct State * state);
void player_play_prepare_free(struct Player *player);
void player_play_prepare_free(struct Player *player);
void player_alloc_queues_free(struct State *state);
void player_sws_context_free(struct Player *player);
int player_alloc_frames_free(struct Player *player);
void player_alloc_video_frames_free(struct Player *player);
void player_find_streams_free(struct Player *player);
void player_open_stream_free(struct Player *player, int stream_no);
void player_open_input_free(struct Player *player);

void player_assign_to_no_boolean_array(struct Player *player, int* array, int value);
int player_if_all_no_array_elements_has_value(struct Player *player, int *array, int value);

void player_stop(struct State * state);



//inline int64_t player_get_current_video_time(struct Player *player);
void player_update_current_time(struct State *state, int64_t current_time, int64_t video_duration, int is_finished);
void player_update_time(struct State *state, int is_finished);

void player_play_prepare(struct Player *player);



int64_t player_get_current_video_time(struct Player *player) {
    if (player->pause) {
        return player->pause_time - player->start_time;
    } else {
        int64_t current_time = av_gettime();
        return current_time - player->start_time;
    }
}



struct Player * get_player(JNIEnv *env, jobject thiz) {      // should not be called before jni_player_init





    jclass thisClass = (*env)->GetObjectClass(env, thiz);


    jfieldID fidNumber = (*env)->GetFieldID(env, thisClass, player_m_native_player[0], player_m_native_player[1]);

    if (fidNumber == NULL) {
        LOGE(1, "ERROR_NOT_FOUND_M_NATIVE_PLAYER_FIELD");
        exit(1);

    }

    long number = (*env)->GetLongField(env, thiz, fidNumber);

    if (fidNumber == NULL) {
        LOGE(1, "PLAYER NOT FOUND");
        exit(1);

    }


    return (struct Player *) number;


}


int jni_player_init(JNIEnv *env, jobject thiz) {

    struct Player *player = malloc(sizeof(struct Player));
    memset(player, 0, sizeof(*player));
    player->audio_stream_no = -1;
    player->video_stream_no = -1;

    int err = ERROR_NO_ERROR;

    int ret = (*env)->GetJavaVM(env, &player->get_javavm);
    if (ret) {
        err = ERROR_COULD_NOT_GET_JAVA_VM;
        goto free_player;
    }

    {
        player->thiz = (*env)->NewGlobalRef(env, thiz);
        if (player->thiz == NULL) {
            err = ERROR_COULD_NOT_CREATE_GLOBAL_REF;
            goto free_player;
        }
    }


    jclass thisClass = (*env)->GetObjectClass(env, thiz);


    jfieldID fidNumber = (*env)->GetFieldID(env, thisClass, player_m_native_player[0], player_m_native_player[1]);

    if (fidNumber == NULL) {
        err = ERROR_NOT_FOUND_M_NATIVE_PLAYER_FIELD;
        goto free_player;

    }



    (*env)->SetLongField(env, thiz, fidNumber, (jlong) player);

    goto end;

    free_player:
    if (player->thiz != NULL) {
        (*env)->DeleteGlobalRef(env, player->thiz);
    }

    free(player);

    end:
    if (err != 0) {
        LOGE(1, "initialization error --- this should not be reached");
        return err;
    }



    pthread_mutex_init(&player->mutex_operation, NULL);
    pthread_mutex_init(&player->mutex_interrupt, NULL);
    pthread_mutex_init(&player->mutex_queue, NULL);
    pthread_cond_init(&player->cond_queue, NULL);

    player->playing = FALSE;
    player->pause = FALSE;
    player->stop = TRUE;
    player->flush_video_play = FALSE;
    player->no_audio = TRUE;

    avformat_network_init();
    av_register_all();

//    register_jni_protocol(player->get_javavm);

    return err;

    // after b3cea6 lower part has been erased
}

int jni_player_set_data_source(JNIEnv *env, jobject thiz, const char *file_path) {


    LOGI(1, "url is %s \n", file_path);

    struct Player * player = get_player(env, thiz);


    struct State state = { player: player, env: env};

    LOGE(1, "cap stream no %d \n", player->capture_streams_no);

    return  player_set_data_source(&state, file_path);



    return 0;
}

int player_set_data_source(struct State *state, const char *file_path) {


    struct Player *player = state->player;
    int err = ERROR_NO_ERROR;
    int i;


    pthread_mutex_lock(&player->mutex_operation);

    player_stop_without_lock(state);

    if (player->playing)
        goto end;



    // initial setup
    player->pause = TRUE;
    player->start_time = 0;
    player->pause_time = 0;

    // trying decode video
    if ((err = player_create_context(player)) < 0) {

        goto error;

    }






    if ((err = player_create_interrupt_callback(player)) < 0)
        goto error;



    if ((err = player_open_input(player, file_path)) < 0)
        goto error;



    if ((err = player_find_stream_info(player)) < 0)
        goto error;





    //player_print_video_informations(player, file_path);

    /*
    if ((err = player_print_report_video_streams(state->env, player->thiz,
                                                 player)) < 0)
        goto error;

        */

    if ((player->video_stream_no = player_find_stream(player,
                                                      AVMEDIA_TYPE_VIDEO, -1)) < 0) {
        err = player->video_stream_no;
        goto error;
    }

    /*
    if ((player->audio_stream_no = player_find_stream(player,
                                                      AVMEDIA_TYPE_AUDIO, audio_stream_no)) < 0) {
        err = player->audio_stream_no;
        LOGW(3, "player_set_data_source, Can not find audio stream");
        player->no_audio = TRUE;
    } else {
        player->no_audio = FALSE;
    }
#ifdef SUBTITLES
    if (subtitle_stream_no == NO_STREAM) {
		player->subtitle_stream_no = -1;
	} else {
		if ((player->subtitle_stream_no = player_find_stream(player,
				AVMEDIA_TYPE_SUBTITLE, subtitle_stream_no)) < 0) {
			// if no subtitles - just go without it
		}
	}

	if ((player->subtitle_stream_no >= 0)) {
		err = player_prepare_ass_decoder(player, font_path);
		if (err < 0)
			goto error;
	}

#endif // SUBTITLES
     */
    if ((err = player_alloc_video_frames(player)) < 0) {
        goto error;
    }


    if ((err = player_alloc_frames(player)) < 0)
        goto error;

    if ((err = player_alloc_queues(state)) < 0)
        goto error;

    struct DecoderState video_decoder_state = { stream_no
    : player->video_stream_no, player: player, env:state->env};
    /*
#ifdef SUBTITLES
    if (player->subtitle_stream_no >= 0) {
		struct DecoderState subtitle_decoder_state = { stream_no
				: player->subtitle_stream_no, player: player, env:state->env};
		if ((err = player_prepare_subtitles_queue(&subtitle_decoder_state,
				state)) < 0)
			goto error;
	}
#endif // SUBTITLES

    if (player->no_audio == FALSE) {
        if ((err = player_create_audio_track(player, state)) < 0) {
            goto error;
        }
    }
    */
    player_get_video_duration(player);
    player_update_time(state, FALSE);

    player_play_prepare(player);

    LOGE(1, "done 0x%x", (unsigned int)player);

    if ((err = player_start_decoding_threads(player)) < 0) {
        goto error;
    }

    // SUCCESS
    player->playing = TRUE;
    LOGI(3, "player_set_data_source success");
    goto end;

    error:
    LOGI(3, "player_set_data_source error");



    player_play_prepare_free(player);
    player_start_decoding_threads_free(player);
    /*
    if (player->no_audio == FALSE) {
        player_create_audio_track_free(player, state);
    }
#ifdef SUBTITLES
    player_prepare_subtitles_queue_free(state);
#endif // SUBTITLES
     */
    player_alloc_queues_free(state);
    player_alloc_frames_free(player);
    player_alloc_video_frames_free(player);
    /*
#ifdef SUBTITLES
    player_prepare_ass_decoder_free(player);
#endif // SUBTITLES
    player_print_report_video_streams_free(state->env, player->thiz, player);
     */
    player_find_streams_free(player);
    //player_find_stream_info_free(player);
    player_open_input_free(player);
    player_create_context_free(player);
    /*
#ifdef SUBTITLES
    if (font_path != NULL)
		free(font_path);
#endif // SUBTITLES
     */
    end:
    LOGI(7, "player_set_data_source end");
    pthread_mutex_unlock(&player->mutex_operation);
    return err;





    LOGI(3, "player_set_data_source error");

    player_play_prepare_free(player);
    player_start_decoding_threads_free(player);

    player_alloc_queues_free(state);
    player_alloc_frames_free(player);
    player_alloc_video_frames_free(player);
    player_find_streams_free(player);
    //player_find_stream_info_free(player);
    player_open_input_free(player);
    player_create_context_free(player);
#ifdef SUBTITLES
    if (font_path != NULL)
		free(font_path);
#endif // SUBTITLES


    return -1;

}


void player_stop_without_lock(struct State * state) {
    int ret;
    struct Player *player = state->player;

    if (!player->playing)
        return;
    player->playing = FALSE;

    LOGI(7, "player_stop_without_lock stopping...");

    player_play_prepare_free(player);
    player_start_decoding_threads_free(player);

    player_alloc_queues_free(state);
    player_sws_context_free(player);
    player_alloc_frames_free(player);
    player_alloc_video_frames_free(player);


    player_find_streams_free(player);
    //player_find_stream_info_free(player);
    player_open_input_free(player);
    player_create_context_free(player);
    LOGI(7, "player_stop_without_lock stopped");
}

int player_create_context(struct Player *player) {





    player->input_format_ctx = avformat_alloc_context();        // as the examples showed...we do not need to allocate context
    if (player->input_format_ctx == NULL) {
        LOGE(1, "Could not create AVContext\n");
        return -ERROR_COULD_NOT_CREATE_AVCONTEXT;
    }



    return 0;
}

int player_create_interrupt_callback(struct Player *player) {
    pthread_mutex_lock(&player->mutex_interrupt);
    player->interrupt = FALSE;
    pthread_mutex_unlock(&player->mutex_interrupt);


    player->interrupt_callback =
            (AVIOInterruptCB) {player_ctx_interrupt_callback, player};
    player->input_format_ctx->interrupt_callback = player->interrupt_callback;

    return 0;
}

int player_ctx_interrupt_callback(void *p) {
    int ret = 0;
    struct Player *player = (struct Player*) p;
    pthread_mutex_lock(&player->mutex_interrupt);
    if (player->interrupt) {
        // method is interrupt
        ret = 1;
    }
    pthread_mutex_unlock(&player->mutex_interrupt);
    return ret;
}


int player_open_input(struct Player *player, const char *file_path) {
    int ret;


    int len = strlen(file_path);
    if (len >= MAX_STRLEN)
        return -MAX_STRLEN_LIMIT_EXCEEDED;


    int i=0;

    for (i=0; i<len; i++) {
        player->file_path[i] = file_path[i];
        if (player->file_path[i] == '\n' || player->file_path[i] == ' ') {              // search for white character ......
            player->file_path[i] = 0;
            break;
        }

    }




    if ((ret = avformat_open_input(&(player->input_format_ctx), player->file_path, NULL,
                                   NULL)) < 0) {
        char errbuf[128];
        const char *errbuf_ptr = errbuf;


        if (av_strerror(ret, errbuf, sizeof(errbuf)) < 0)
            errbuf_ptr = strerror(AVUNERROR(ret));

        LOGE(1,
             "player_set_data_source Could not open video file: %s (%d: %s)\n",
             player->file_path, ret, errbuf_ptr);
        return -ERROR_COULD_NOT_OPEN_VIDEO_FILE;
    }
    player->input_inited = TRUE;


    return ERROR_NO_ERROR;
}


int player_find_stream_info(struct Player *player) {
    LOGI(3, "player_set_data_source 2");
    // find video informations
    if (avformat_find_stream_info(player->input_format_ctx, NULL) < 0) {
        LOGE(1, "Could not open stream\n");
        return -ERROR_COULD_NOT_OPEN_STREAM;
    }
    return ERROR_NO_ERROR;
}


int player_find_stream(struct Player *player, enum AVMediaType codec_type,
                       int recommended_stream_no) {

    //find video stream
    int streams_no = player->capture_streams_no;

    int err = ERROR_NO_ERROR;
    LOGI(3, "player_find_stream, type: %d", codec_type);

    int bn_stream = player_try_open_stream(player, codec_type,
                                           recommended_stream_no);

    if (bn_stream < 0) {
        int i;
        for (i = 0; i < player->input_format_ctx->nb_streams; i++) {
            bn_stream = player_try_open_stream(player, codec_type, i);
            if (bn_stream >= 0)
                break;
        }
    }

    if (bn_stream < 0) {
        return -1;
    }

    LOGI(3, "player_set_data_source 4");

    AVStream *stream = player->input_format_ctx->streams[bn_stream];
    player->input_streams[streams_no] = stream;
    player->input_codec_ctxs[streams_no] = stream->codec;
    player->input_stream_numbers[streams_no] = bn_stream;

    if (codec_type == AVMEDIA_TYPE_VIDEO) {
        LOGI(5,
             "player_set_data_source Video size is [%d x %d]",
             player->input_codec_ctxs[streams_no]->width,
             player->input_codec_ctxs[streams_no]->height);
    }

    player->capture_streams_no += 1;
    return streams_no;


}

int player_try_open_stream(struct Player *player, enum AVMediaType codec_type,
                           int stream_no) {
    if (stream_no < 0)
        return -1;
    if (stream_no >= player->input_format_ctx->nb_streams)
        return -1;

    AVStream *stream = player->input_format_ctx->streams[stream_no];
    AVCodecContext *ctx = stream->codec;
    if (ctx->codec_type != codec_type) {
        return -1;
    }

    const struct AVCodec * codec = ctx->codec;
    int err = player_open_stream(player, ctx, &codec, stream_no);
    if (err < 0) {
        return -1;
    }

    return stream_no;
}

int player_open_stream(struct Player *player, AVCodecContext * ctx,
                       const struct AVCodec **codec, int stream_no) {
    enum AVCodecID codec_id = ctx->codec_id;
    LOGI(3, "player_open_stream trying open: %d", codec_id);

    *codec = avcodec_find_decoder(codec_id);
    if (*codec == NULL) {
        LOGE(1,
             "player_set_data_source Could not find codec for id: %d",
             codec_id);
        return -ERROR_COULD_NOT_FIND_VIDEO_CODEC;
    }

    //enable multi-thread decode video,
    //the thread_count should be match with the cpu cores,
    //you can get this form java or use cpufeature lib
    //here I am fixed it with 4.

    if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        ctx->thread_count = 4;
    }


    if (avcodec_open2(ctx, *codec, NULL) < 0) {
        LOGE(1, "Could not open codec");
        //player_print_codec_description(*codec);
        *codec = NULL;
        return -ERROR_COULD_NOT_OPEN_VIDEO_CODEC;
    }
    LOGI(3,
         "player_open_stream opened: %d, name: %s, long_name: %s",
         codec_id, (*codec)->name, (*codec)->long_name);
    return 0;
}

int player_alloc_video_frames(struct Player *player) {
    player->rgb_frame = av_frame_alloc();
    if (player->rgb_frame == NULL) {
        LOGE(1, "player_alloc_video_frames could not allocate rgb_frame");
        return -1;
    }

    player->tmp_frame = av_frame_alloc();
    if (player->tmp_frame == NULL) {
        LOGE(1, "player_alloc_video_frames could not allocate tmp_frame");
        return -1;
    }
    player->tmp_frame2 = av_frame_alloc();
    if (player->tmp_frame2 == NULL) {
        LOGE(1, "player_alloc_video_frames could not allocate tmp_frame2");
        return -1;
    }
    AVCodecContext * ctx = player->input_codec_ctxs[player->video_stream_no];
    int numBytes = avpicture_get_size(AV_PIX_FMT_RGBA, ctx->width, ctx->height);
    player->tmp_buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    if (player->tmp_buffer == NULL) {
        LOGE(1, "player_alloc_video_frames could not allocate tmp_buffer");
        return -1;
    }
    player->tmp_buffer2 = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    if (player->tmp_buffer2 == NULL) {
        LOGE(1, "player_alloc_video_frames could not allocate tmp_buffer2");
        return -1;
    }
    avpicture_fill((AVPicture *) player->tmp_frame, player->tmp_buffer,
                   AV_PIX_FMT_RGBA, ctx->width, ctx->height);
    avpicture_fill((AVPicture *) player->tmp_frame2, player->tmp_buffer2,
                   AV_PIX_FMT_RGBA, ctx->width, ctx->height);
    LOGI(3, "Allocating: %dx%d", ctx->width, ctx->height);
    return 0;
}

void player_alloc_video_frames_free(struct Player *player) {
    if (player->rgb_frame != NULL) {
        av_frame_free(&player->rgb_frame);
        player->rgb_frame = NULL;
    }
    if (player->tmp_frame != NULL) {
        av_frame_free(&player->tmp_frame);
        player->tmp_frame = NULL;
    }
    if (player->tmp_frame2 != NULL) {
        av_frame_free(&player->tmp_frame2);
        player->tmp_frame2 = NULL;
    }
    if (player->tmp_buffer != NULL) {
        av_free(player->tmp_buffer);
        player->tmp_buffer = NULL;
    }
    if (player->tmp_buffer2 != NULL) {
        av_free(player->tmp_buffer2);
        player->tmp_buffer2 = NULL;
    }
}

int player_alloc_frames(struct Player *player) {
    int capture_streams_no = player->capture_streams_no;
    int stream_no;
    for (stream_no = 0; stream_no < capture_streams_no; ++stream_no) {
        player->input_frames[stream_no] = av_frame_alloc();
        if (player->input_frames[stream_no] == NULL) {
            return -ERROR_COULD_NOT_ALLOC_FRAME;
        }
    }
    return 0;
}

int player_alloc_queues(struct State *state) {
    struct Player *player = state->player;
    int capture_streams_no = player->capture_streams_no;
    int stream_no;
    for (stream_no = 0; stream_no < capture_streams_no; ++stream_no) {
        player->packets[stream_no] = queue_init_with_custom_lock(1000,
                                                                 (queue_fill_func) player_fill_packet,
                                                                 (queue_free_func) player_free_packet, state, state,
                                                                 &player->mutex_queue, &player->cond_queue);
        if (player->packets[stream_no] == NULL) {
            return -ERROR_COULD_NOT_PREPARE_PACKETS_QUEUE;
        }
    }
    return 0;
}

void *player_fill_packet(struct State *state) {
    struct PacketData *packet_data = malloc(sizeof (struct PacketData));
    if (packet_data == NULL) {
        return NULL;
    }
    packet_data->packet = malloc(sizeof(AVPacket));
    if (packet_data->packet == NULL) {
        free(packet_data);
        return NULL;
    }
    return packet_data;
}

void player_free_packet(struct State *player, struct PacketData *elem) {
    free(elem->packet);
    free(elem);
}






void player_play_prepare_free(struct Player *player) {
    pthread_mutex_lock(&player->mutex_queue);
    player->stop = TRUE;
    pthread_cond_broadcast(&player->cond_queue);
    pthread_mutex_unlock(&player->mutex_queue);
}


int player_start_decoding_threads_free(struct Player *player) {
    int err = 0;
    int ret;
    int i;
    if (player->thread_player_read_from_stream_created) {
        ret = pthread_join(player->thread_player_read_from_stream, NULL);
        player->thread_player_read_from_stream_created = FALSE;
        if (ret) {
            err = ERROR_COULD_NOT_JOIN_PTHREAD;
        }
    }

    for (i = 0; i < player->capture_streams_no; ++i) {
        if (player->decode_threads_created[i]) {
            ret = pthread_join(player->decode_threads[i], NULL);
            player->decode_threads_created[i] = FALSE;
            if (ret) {
                err = ERROR_COULD_NOT_JOIN_PTHREAD;
            }
        }
    }
    return err;
}

void player_alloc_queues_free(struct State *state) {
    struct Player *player = state->player;
    int capture_streams_no = player->capture_streams_no;
    int stream_no;
    for (stream_no = 0; stream_no < capture_streams_no; ++stream_no) {
        if (player->packets[stream_no] != NULL) {
            queue_free(player->packets[stream_no], &player->mutex_queue,
                       &player->cond_queue, state);
            player->packets[stream_no] = NULL;
        }
    }
}

void player_sws_context_free(struct Player *player) {
    if (player->sws_context != NULL) {
        sws_freeContext(player->sws_context);
        player->sws_context = NULL;
    }
}

int player_alloc_frames_free(struct Player *player) {
    int capture_streams_no = player->capture_streams_no;
    int stream_no;
    for (stream_no = 0; stream_no < capture_streams_no; ++stream_no) {
        if (player->input_frames[stream_no] != NULL) {
            av_free(player->input_frames[stream_no]);
            player->input_frames[stream_no] = NULL;
        }
    }
    return 0;
}




void player_find_streams_free(struct Player *player) {
    int capture_streams_no = player->capture_streams_no;
    int i;
    for (i = 0; i < capture_streams_no; ++i) {
        player_open_stream_free(player, i);
    }
    player->capture_streams_no = 0;
    player->video_stream_no = -1;
    player->audio_stream_no = -1;
}

void player_open_stream_free(struct Player *player, int stream_no) {
    AVCodecContext ** ctx = &player->input_codec_ctxs[stream_no];
    if (*ctx != NULL) {
        avcodec_close(*ctx);
        *ctx = NULL;
    }
}

void player_open_input_free(struct Player *player) {
    if (player->input_inited) {
        LOGI(7, "player_set_data_source close_file");
        avformat_close_input(&(player->input_format_ctx));
        player->input_inited = FALSE;
    }
}

void player_create_context_free(struct Player *player) {
    if (player->input_format_ctx != NULL) {
        LOGI(7, "player_set_data_source remove_context");
        av_free(player->input_format_ctx);
        player->input_format_ctx = NULL;
    }
}


void player_get_video_duration(struct Player *player) {
    player->last_updated_time = -1;
    player->video_duration = 0;
    int i;

    for (i = 0; i < player->capture_streams_no; ++i) {
        AVStream *stream = player->input_streams[i];
        if (stream->duration > 0) {
            player->video_duration = av_rescale_q(
                    stream->duration, stream->time_base, AV_TIME_BASE_Q);
            LOGI(3,
                 "player_set_data_source stream[%d] duration: %ld",
                 i, stream->duration);
            return;
        }
    }
    if (player->input_format_ctx->duration != 0) {
        player->video_duration = player->input_format_ctx->duration;
        LOGI(3,
             "player_set_data_source video duration: %ld",
             player->input_format_ctx->duration)
        return;
    }

    for (i = 0; i < player->input_format_ctx->nb_streams; i++) {
        AVStream *stream = player->input_format_ctx->streams[i];
        if (stream->duration > 0) {
            player->video_duration = av_rescale_q(stream->duration,
                                                  stream->time_base, AV_TIME_BASE_Q);
            LOGI(3,
                 "player_set_data_source stream[%d] duration: %ld", i, stream->duration);
            return;
        }
    }
}

void player_update_time(struct State *state, int is_finished) {
    struct Player *player = state->player;
    int inform_user = FALSE;

    pthread_mutex_lock(&player->mutex_queue);
    int64_t current_video_time = player_get_current_video_time(player);
    int64_t time_diff = player->last_updated_time - current_video_time;

    if (is_finished) {
        inform_user = TRUE;
    }

    if (time_diff > 500000ll || time_diff < -500000ll) {
        player->last_updated_time = current_video_time;
        inform_user = TRUE;
    }

    int64_t video_duration = player->video_duration;
    pthread_mutex_unlock(&player->mutex_queue);

    LOGI(6, "player_update_time: %f/%f",
         current_video_time/1000000.0, video_duration/1000000.0);

    if (inform_user) {
        // because video duration can be estimate we have to ensure that
        // it will not be smaller than current time
        if (current_video_time > video_duration) {
            video_duration = current_video_time;
        }
        player_update_current_time(state, current_video_time,
                                   video_duration, is_finished);
    }
}


/*      Just to see the a sample
 *  jclass thisClass = (*env)->GetObjectClass(env, player->thiz);
    jmethodID midCallBack = (*env)->GetMethodID(env, thisClass, "callbackError", "(I)V");

    if (NULL == midCallBack) {
        LOGE(0, "callback error");
        return;
    }
    jint error = player->error;
    LOGE(1, "calling callback with %d", error);
    (*env)->CallVoidMethod(env, player->thiz, midCallBack, error);

 */
void player_update_current_time(struct State *state,
                                int64_t current_time,
                                int64_t video_duration,
                                int is_finished) {
    struct Player *player = state->player;
    jboolean jis_finished = is_finished ? JNI_TRUE : JNI_FALSE;

    LOGI(4, "player_update_current_time: %f/%f",
         current_time/1000000.0, video_duration/1000000.0);
    /*
    (*state->env)->CallVoidMethod(state->env, player->thiz,
                                  player->player_on_update_time_method, current_time,
                                  video_duration, jis_finished);
                                  */

    jclass thisClass = (*(state->env))->GetObjectClass(state->env, player->thiz);
    jmethodID onUpdateTimeMethod = (*(state->env))->GetMethodID(state->env, thisClass, "onUpdateTime", "(JJZ)V");

    if (NULL == onUpdateTimeMethod) {
        player->error = ON_UPDATE_TIME_METHOD_NOT_FOUND;
        callback(player, state->env);

    } else {
        (*(state->env))->CallVoidMethod(state->env, player->thiz, onUpdateTimeMethod, current_time,
                                        video_duration, jis_finished);
    }




}

void player_play_prepare(struct Player *player) {
    LOGI(3, "player_set_data_source 16");
    pthread_mutex_lock(&player->mutex_queue);
    player->stop = FALSE;
    player_assign_to_no_boolean_array(player, player->flush_streams, FALSE);
    player_assign_to_no_boolean_array(player, player->stop_streams, FALSE);

    pthread_cond_broadcast(&player->cond_queue);
    pthread_mutex_unlock(&player->mutex_queue);
}

void player_assign_to_no_boolean_array(struct Player *player, int* array,
                                              int value) {
    int capture_streams_no = player->capture_streams_no;
    int stream_no;
    for (stream_no = 0; stream_no < capture_streams_no; ++stream_no) {
        array[stream_no] = value;
    }
}

int player_if_all_no_array_elements_has_value(struct Player *player,
                                                     int *array, int value) {
    int capture_streams_no = player->capture_streams_no;
    int stream_no;
    for (stream_no = 0; stream_no < capture_streams_no; ++stream_no) {
        if (array[stream_no] != value)
            return FALSE;
    }
    return TRUE;
}


void jni_player_resume(JNIEnv *env, jobject thiz) {
    struct Player * player = get_player(env, thiz);
    pthread_mutex_lock(&player->mutex_operation);


    if (!player->playing) {
    /*    LOGI(1, "jni_player_resume could not pause while not playing");
        throw_exception(env, not_playing_exception_class_path_name,
                        "Could not resume while not playing");*/
        LOGE(1, "error to resume!!!");
        goto end;
    }

    pthread_mutex_lock(&player->mutex_queue);

    if (!player->pause) {
        LOGE(1, "already pause is FALSE");
        goto do_nothing;
    }

    player->pause = FALSE;

    int64_t resume_time = av_gettime();
    player->start_time += resume_time - player->pause_time;

    pthread_cond_broadcast(&player->cond_queue);

    /*
    if (player->no_audio == FALSE) {
        (*env)->CallVoidMethod(env, player->audio_track,
                               player->audio_track_play_method);
        // just leave exception
    }
     */

    do_nothing:
    pthread_mutex_unlock(&player->mutex_queue);

    end:
    pthread_mutex_unlock(&player->mutex_operation);
}

void jni_player_dealloc(JNIEnv *env, jobject thiz) {


    struct Player *player = get_player(env, thiz);

    if(player == NULL) {
        LOGE(1, "player already deallocated");
        return;
    }

    if (player->thiz != NULL) {
        (*env)->DeleteGlobalRef(env, player->thiz);
    }
    /*
    if (player->audio_track_class != NULL) {
        (*env)->DeleteGlobalRef(env, player->audio_track_class);
    }
     */
    free(player);

}

void jni_player_stop(JNIEnv *env, jobject thiz) {

    struct Player * player = get_player(env, thiz);
    struct State state = {player: player, env: env};
    player_stop(&state);
}

void player_stop(struct State * state) {
    int ret;

    struct Player * player = state->player;

    pthread_mutex_lock(&player->mutex_interrupt);
    player->interrupt = TRUE;
    pthread_mutex_unlock(&player->mutex_interrupt);

    pthread_mutex_lock(&player->mutex_operation);
    if(state->player->playing == TRUE)
        player_stop_without_lock(state);

    pthread_mutex_unlock(&player->mutex_operation);
}


void jni_player_render(JNIEnv *env, jobject thiz, jobject surface) {

    struct Player * player = get_player(env, thiz);
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);

    LOGI(4, "jni_player_render")
    pthread_mutex_lock(&player->mutex_queue);
    if (player->window != NULL) {
        LOGE(1,
             "jni_player_render Window have to be null before "
                     "calling render function");
        exit(1);
    }
    ANativeWindow_acquire(window);
    player->window = window;
    pthread_cond_broadcast(&player->cond_queue);
    pthread_mutex_unlock(&player->mutex_queue);
}

void jni_player_render_frame_stop(JNIEnv *env, jobject thiz) {
    struct Player * player = get_player(env, thiz);

    struct State state = {player: player, env: env};

    LOGI(5, "jni_player_render_frame_stop stopping render");

    player_stop(&state);

    LOGI(5, "jni_player_render_frame_stop waiting for mutex");
    pthread_mutex_lock(&player->mutex_queue);
    if (player->window == NULL) {
        LOGE(1,
             "jni_player_render_frame_stop Window is null this "
                     "mean that you did not call render function");
        exit(1);
    }
    LOGI(5, "jni_player_render_frame_stop releasing window");
    ANativeWindow_release(player->window);
    player->window = NULL;
    pthread_cond_broadcast(&player->cond_queue);
    pthread_mutex_unlock(&player->mutex_queue);
}

void callback(struct Player *player, JNIEnv *env) {
    jclass thisClass = (*env)->GetObjectClass(env, player->thiz);
    jmethodID midCallBack = (*env)->GetMethodID(env, thisClass, "callbackError", "(I)V");

    if (NULL == midCallBack) {
        LOGE(0, "callback error");
        return;
    }
    jint error = player->error;
    LOGE(1, "calling callback with %d", error);
    (*env)->CallVoidMethod(env, player->thiz, midCallBack, error);
}