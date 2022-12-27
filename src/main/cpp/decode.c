//

#include <android/log.h>
#include <assert.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include "player.h"
#include "sync.h"
#include "convert.h"


void player_update_time(struct State *state, int is_finished);
void player_assign_to_no_boolean_array(struct Player *player, int* array, int value);
int player_if_all_no_array_elements_has_value(struct Player *player, int *array, int value);
// Created by usrc on 16. 12. 14.
//

enum DecodeCheckMsg {
    DECODE_CHECK_MSG_STOP = 0, DECODE_CHECK_MSG_FLUSH,
};

enum ReadFromStreamCheckMsg {
    READ_FROM_STREAM_CHECK_MSG_STOP = 0, READ_FROM_STREAM_CHECK_MSG_SEEK,
};

void * player_decode(void * data);
QueueCheckFuncRet player_decode_queue_check_func(Queue *queue, struct DecoderData *decoderData, int *ret);
int player_decode_video(struct DecoderData * decoder_data, JNIEnv * env, struct PacketData *packet_data);
void player_decode_video_flush(struct DecoderData * decoder_data, JNIEnv * env);
void * player_read_from_stream(void *data);
enum WaitFuncRet player_wait_for_frame(struct Player *player, int64_t stream_time, int stream_no);
QueueCheckFuncRet player_read_from_stream_check_func(Queue *queue, struct Player *player, int *ret);
static void ms2ts(struct timespec *ts, unsigned long ms);


int player_start_decoding_threads(struct Player *player) {

    pthread_attr_t attr;
    int ret;
    int i;
    int err = 0;
    ret = pthread_attr_init(&attr);
    if (ret) {
        err = -ERROR_COULD_NOT_INIT_PTHREAD_ATTR;
        goto end;
    }
    for (i = 0; i < player->capture_streams_no; ++i) {
        struct DecoderData * decoder_data = malloc(sizeof(decoder_data));
        *decoder_data = (struct DecoderData) {player: player, stream_no: i};
        ret = pthread_create(&player->decode_threads[i], &attr, player_decode,
                             decoder_data);
        if (ret) {
            err = -ERROR_COULD_NOT_CREATE_PTHREAD;
            goto end;
        }
        player->decode_threads_created[i] = TRUE;
    }

    ret = pthread_create(&player->thread_player_read_from_stream, &attr,
                         player_read_from_stream, player);
    if (ret) {
        err = -ERROR_COULD_NOT_CREATE_PTHREAD;
        goto end;
    }
    player->thread_player_read_from_stream_created = TRUE;

    end: ret = pthread_attr_destroy(&attr);
    if (ret) {
        if (!err) {
            err = ERROR_COULD_NOT_DESTROY_PTHREAD_ATTR;
        }
    }
    return err;


}

void * player_decode(void * data) {

    int err = ERROR_NO_ERROR;
    struct DecoderData *decoder_data = data;
    struct Player *player = decoder_data->player;
    int stream_no = decoder_data->stream_no;
    Queue *queue = player->packets[stream_no];
    AVCodecContext * ctx = player->input_codec_ctxs[stream_no];
    enum AVMediaType codec_type = ctx->codec_type;

    int stop = FALSE;
    JNIEnv * env;
    char thread_title[256];
    sprintf(thread_title, "FFmpegDecode[%d]", stream_no);

    JavaVMAttachArgs thread_spec = { JNI_VERSION_1_4, thread_title, NULL };

    jint ret = (*player->get_javavm)->AttachCurrentThread(player->get_javavm,
                                                          &env, &thread_spec);
    if (ret || env == NULL) {
        err = -ERROR_COULD_NOT_ATTACH_THREAD;
        goto end;
    }


    for (;;) {
        LOGI(10, "player_decode waiting for frame[%d]", stream_no);
        int interrupt_ret;
        struct PacketData *packet_data;
        pthread_mutex_lock(&player->mutex_queue);
        pop: packet_data = queue_pop_start_already_locked(&queue,
                                                          &player->mutex_queue, &player->cond_queue,
                                                          (QueueCheckFunc) player_decode_queue_check_func, decoder_data,
                                                          (void **) &interrupt_ret);

        if (packet_data == NULL) {
            if (interrupt_ret == DECODE_CHECK_MSG_FLUSH) {
                goto flush;
            } else if (interrupt_ret == DECODE_CHECK_MSG_STOP) {
                goto stop;
            } else {
                assert(FALSE);
            }
        }
        pthread_mutex_unlock(&player->mutex_queue);
        LOGI(10, "player_decode decoding frame[%d]", stream_no);
        if (packet_data->end_of_stream) {
            LOGI(10, "player_decode read end of stream");
        }

#ifdef MEASURE_TIME
        struct timespec timespec1, timespec2;
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timespec1);
#endif // MEASURE_TIME
        if (codec_type == AVMEDIA_TYPE_AUDIO) {
            // Some audio sources need to be decoded multiple times when packet buffer is not empty
            /*
            while(err >= 0 && packet_data->packet->size > 0) {
                err = player_decode_audio(decoder_data, env, packet_data);
            }
             */
        } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
            err = player_decode_video(decoder_data, env, packet_data);
        } else
            /*
#ifdef SUBTITLES
            if (codec_type == AVMEDIA_TYPE_SUBTITLE) {
			err = player_decode_subtitles(decoder_data, env, packet_data);
		} else
#endif // SUBTITLES */
        {
            assert(FALSE);
        }

        struct State state = {player: player, env:env};
        player_update_time(&state, packet_data->end_of_stream);

#ifdef MEASURE_TIME
        char * type = "unknown";
		if (codec_type == AVMEDIA_TYPE_AUDIO) {
			type = "audio";
		} else if (codec_type == AVMEDIA_TYPE_VIDEO) {
			type = "video";
		} else if (codec_type == AVMEDIA_TYPE_SUBTITLE) {
			type = "subtitle";
		}
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timespec2);
		struct timespec diff = timespec_diff(timespec1, timespec2);
		LOGI(7,
				"decode timediff (%s): %d.%9ld", type, diff.tv_sec, diff.tv_nsec);
#endif // MEASURE_TIME
        if (!packet_data->end_of_stream) {
            av_free_packet(packet_data->packet);
        }
        queue_pop_finish(queue, &player->mutex_queue, &player->cond_queue);
        if (err < 0) {
            pthread_mutex_lock(&player->mutex_queue);
            goto stop;
        }

        goto end_loop;

        stop:
        LOGI(2, "player_decode stop[%d]", stream_no);
        stop = TRUE;

        flush:
        LOGI(2, "player_decode flush[%d]", stream_no);
        struct PacketData *to_free;
        while ((to_free = queue_pop_start_already_locked_non_block(queue))
               != NULL) {
            if (!to_free->end_of_stream) {
                av_free_packet(to_free->packet);
            }
            queue_pop_finish_already_locked(queue, &player->mutex_queue,
                                            &player->cond_queue);
        }
        LOGI(2, "player_decode flushing playback[%d]", stream_no);

        if (codec_type == AVMEDIA_TYPE_AUDIO) {
            //player_decode_audio_flush(decoder_data, env);
        } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
            player_decode_video_flush(decoder_data, env);
        } else
            /*
#ifdef SUBTITLES
            if (codec_type == AVMEDIA_TYPE_SUBTITLE) {
			player_decode_subtitles_flush(decoder_data, env);
		} else
#endif // SUBTITLES
             */
        {
            assert(FALSE);
        }
        LOGI(2, "player_decode flushed playback[%d]", stream_no);

        if (stop) {
            LOGI(2, "player_decode stopping stream");
            player->stop_streams[stream_no] = FALSE;
            pthread_cond_broadcast(&player->cond_queue);
            pthread_mutex_unlock(&player->mutex_queue);
            goto detach_current_thread;
        } else {
            LOGI(2, "player_decode flush stream[%d]", stream_no);
            player->flush_streams[stream_no] = FALSE;
            pthread_cond_broadcast(&player->cond_queue);
            goto pop;
        }
        end_loop: continue;
    }

    detach_current_thread: ret = (*player->get_javavm)->DetachCurrentThread(
            player->get_javavm);
    if (ret && !err)
        err = ERROR_COULD_NOT_DETACH_THREAD;

    end: free(decoder_data);
    decoder_data = NULL;

    // TODO do something with err
    return NULL;
}

QueueCheckFuncRet player_decode_queue_check_func(Queue *queue,
                                                 struct DecoderData *decoderData, int *ret) {
    struct Player *player = decoderData->player;
    int stream_no = decoderData->stream_no;
    if (player->stop_streams[stream_no]) {
        *ret = DECODE_CHECK_MSG_STOP;
        return QUEUE_CHECK_FUNC_RET_SKIP;
    }
    if (player->flush_streams[stream_no]) {
        *ret = DECODE_CHECK_MSG_FLUSH;
        return QUEUE_CHECK_FUNC_RET_SKIP;
    }
    return QUEUE_CHECK_FUNC_RET_TEST;
}


int player_decode_video(struct DecoderData * decoder_data, JNIEnv * env,
                        struct PacketData *packet_data) {
    int got_frame_ptr;
    struct Player *player = decoder_data->player;
    int stream_no = decoder_data->stream_no;
    AVCodecContext * ctx = player->input_codec_ctxs[stream_no];
    AVFrame * frame = player->input_frames[stream_no];
    AVStream * stream = player->input_streams[stream_no];
    int interrupt_ret;
    int to_write;
    int err = 0;
    AVFrame *rgb_frame = player->rgb_frame;
    ANativeWindow_Buffer buffer;
    ANativeWindow * window;

#ifdef MEASURE_TIME
    struct timespec timespec1, timespec2, diff;
#endif // MEASURE_TIME
    LOGI(10, "player_decode_video decoding");
    int frameFinished;

#ifdef MEASURE_TIME
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timespec1);
#endif // MEASURE_TIME
    int ret = avcodec_decode_video2(ctx, frame, &frameFinished,
                                    packet_data->packet);

#ifdef MEASURE_TIME
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timespec2);
	diff = timespec_diff(timespec1, timespec2);
	LOGI(3, "decode_video timediff: %d.%9ld", diff.tv_sec, diff.tv_nsec);
#endif // MEASURE_TIME

    if (ret < 0) {
        LOGE(1, "player_decode_video Fail decoding video %d\n", ret);
        return -ERROR_WHILE_DECODING_VIDEO;
    }
    if (!frameFinished) {
        LOGI(10, "player_decode_video Video frame not finished\n");
        return 0;
    }

    // saving in buffer converted video frame
    LOGI(7, "player_decode_video copy wait");

#ifdef MEASURE_TIME
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timespec1);
#endif // MEASURE_TIME

    pthread_mutex_lock(&player->mutex_queue);
    window = player->window;
    if (window == NULL) {
        pthread_mutex_unlock(&player->mutex_queue);
        goto skip_frame;
    }
    ANativeWindow_setBuffersGeometry(window, ctx->width, ctx->height,
                                     WINDOW_FORMAT_RGBA_8888);
    if (ANativeWindow_lock(window, &buffer, NULL) != 0) {
        pthread_mutex_unlock(&player->mutex_queue);
        goto skip_frame;
    }
    pthread_mutex_unlock(&player->mutex_queue);

    int format = buffer.format;
    if (format < 0) {
        LOGE(1, "Could not get window format")
    }
    enum AVPixelFormat out_format;
    if (format == WINDOW_FORMAT_RGBA_8888) {
        out_format = AV_PIX_FMT_RGBA;
        LOGI(6, "Format: WINDOW_FORMAT_RGBA_8888");
    } else if (format == WINDOW_FORMAT_RGBX_8888) {
        out_format = AV_PIX_FMT_RGB0;
        LOGE(1, "Format: WINDOW_FORMAT_RGBX_8888 (not supported)");
    } else if (format == WINDOW_FORMAT_RGB_565) {
        out_format = AV_PIX_FMT_RGB565;
        LOGE(1, "Format: WINDOW_FORMAT_RGB_565 (not supported)");
    } else {
        LOGE(1, "Unknown window format");
    }

    avpicture_fill((AVPicture *) rgb_frame, buffer.bits, out_format,
                   buffer.width, buffer.height);
    rgb_frame->data[0] = buffer.bits;
    if (format == WINDOW_FORMAT_RGBA_8888) {
        rgb_frame->linesize[0] = buffer.stride * 4;
    } else {
        LOGE(1, "Unknown window format");
    }
    LOGI(6,
         "Buffer: width: %d, height: %d, stride: %d",
         buffer.width, buffer.height, buffer.stride);
    int i = 0;

#ifdef MEASURE_TIME
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timespec2);
	diff = timespec_diff(timespec1, timespec2);
	LOGI(1,
			"lockPixels and fillimage timediff: %d.%9ld", diff.tv_sec, diff.tv_nsec);
#endif // MEASURE_TIME
#ifdef MEASURE_TIME
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timespec1);
#endif // MEASURE_TIME
    LOGI(7, "player_decode_video copying...");
    AVFrame * out_frame;
    int rescale;
    if (ctx->width == buffer.width && ctx->height == buffer.height) {
        // This always should be true
        out_frame = rgb_frame;
        rescale = FALSE;
    } else {
        out_frame = player->tmp_frame2;
        rescale = TRUE;
        LOGE(1, "rescale found");
    }


    if (ctx->pix_fmt == AV_PIX_FMT_YUV420P) {
        __I420ToARGB(frame->data[0], frame->linesize[0], frame->data[2],
                     frame->linesize[2], frame->data[1], frame->linesize[1],
                     out_frame->data[0], out_frame->linesize[0], ctx->width,
                     ctx->height);
    } else if (ctx->pix_fmt == AV_PIX_FMT_NV12) {
        __NV21ToARGB(frame->data[0], frame->linesize[0], frame->data[1],
                     frame->linesize[1], out_frame->data[0], out_frame->linesize[0],
                     ctx->width, ctx->height);
    } else {
        LOGI(3, "Using slow conversion: %d ", ctx->pix_fmt);
        struct SwsContext *sws_context = player->sws_context;
        sws_context = sws_getCachedContext(sws_context, ctx->width, ctx->height,
                                           ctx->pix_fmt, ctx->width, ctx->height, out_format,
                                           SWS_FAST_BILINEAR, NULL, NULL, NULL);
        player->sws_context = sws_context;
        if (sws_context == NULL) {
            LOGE(1, "could not initialize conversion context from: %d"
                    ", to :%d\n", ctx->pix_fmt, out_format);
            // TODO some error
        }
        sws_scale(sws_context, (const uint8_t * const *) frame->data,
                  frame->linesize, 0, ctx->height, out_frame->data,
                  out_frame->linesize);
    }


    if (rescale) {
        // Never occurs
        __ARGBScale(out_frame->data[0], out_frame->linesize[0], ctx->width,
                    ctx->height, rgb_frame->data[0], rgb_frame->linesize[0],
                    buffer.width, buffer.height, __kFilterNone);
        out_frame = rgb_frame;
    }


    int64_t pts = av_frame_get_best_effort_timestamp(frame);
    if (pts == AV_NOPTS_VALUE) {
        pts = 0;
        LOGE(1, "severe error");
    }
    int64_t time = av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);
    LOGI(10,
         "player_decode_video Decoded video frame: %f, time_base: %" SCNd64,
         time/1000000.0, pts);


    if (time > 10*1000000.0 && !player->error && 0) {
        player->error = ERROR_PTS_IS_ZERO;
        callback(player, env);

    }



    player_wait_for_frame(player, time, stream_no);


#ifdef SUBTITLES

    if ((player->subtitle_stream_no >= 0)) {
//		double timeDouble = (double) pts * av_q2d(stream->time_base);
		pthread_mutex_lock(&player->mutex_queue);
		struct SubtitleElem * subtitle = NULL;
		// there is no subtitles in this video
		for (;;) {
			subtitle = queue_pop_start_already_locked_non_block(
					player->subtitles_queue);
			LOGI(5, "player_decode_video reading subtitle");
			if (subtitle == NULL) {
				LOGI(5, "player_decode_video no more subtitles found");
				break;
			}
			if (subtitle->stop_time >= time)
				break;
			avsubtitle_free(&subtitle->subtitle);
			subtitle = NULL;
			LOGI(5, "player_decode_video discarding old subtitle");
			queue_pop_finish_already_locked(player->subtitles_queue,
					&player->mutex_queue, &player->cond_queue);
		}

		if (subtitle != NULL) {
			if (subtitle->start_time > time) {
				LOGI(5,
						"player_decode_video rollback too new subtitle: %f > %f",
						subtitle->start_time/1000000.0, time/1000000.0);
				queue_pop_roll_back_already_locked(player->subtitles_queue,
						&player->mutex_queue, &player->cond_queue);
				subtitle = NULL;
			}
		}

		pthread_mutex_unlock(&player->mutex_queue);


		/* libass stores an RGBA color in the format RRGGBBAA,
		 * where AA is the transparency level */
		if (subtitle != NULL) {
			LOGI(5, "player_decode_video blend subtitle");
			int i;
			struct AVSubtitle *sub = &subtitle->subtitle;
			for (i = 0; i < sub->num_rects; i++) {
				AVSubtitleRect *rect = sub->rects[i];
				if (rect->type != SUBTITLE_BITMAP) {
					continue;
				}
				LOGI(5, "player_decode_video blending subtitle");
				blend_subrect_rgba((AVPicture *) out_frame, rect, buffer.width,
						buffer.height, out_format);
			}
		}
		int64_t time_ms = time / 1000;

		LOGI(3,
				"player_decode_video_subtitles: trying to find subtitles in : %" SCNd64,
				time_ms);
		pthread_mutex_lock(&player->mutex_ass);
		ASS_Image *image = ass_render_frame(player->ass_renderer,
				player->ass_track, time_ms, NULL);
		for (; image != NULL; image = image->next) {
			LOGI(3,
					"player_decode_video_subtitles: printing subtitles in : %" SCNd64,
					time_ms);
			blend_ass_image((AVPicture *) out_frame, image, buffer.width,
					buffer.height, out_format);
		}
		pthread_mutex_unlock(&player->mutex_ass);

		pthread_mutex_lock(&player->mutex_queue);
		if (subtitle != NULL) {
			LOGI(5, "player_decode_video rollback wroten subtitle");
			queue_pop_roll_back_already_locked(player->subtitles_queue, &player->mutex_queue,
					&player->cond_queue);
			subtitle = NULL;
		}
		pthread_mutex_unlock(&player->mutex_queue);
	}
#endif // SUBTITLES

    ANativeWindow_unlockAndPost(window);
    skip_frame:
    return err;
}


enum WaitFuncRet player_wait_for_frame(struct Player *player, int64_t stream_time,
                                       int stream_no) {
    LOGI(6, "player_wait_for_frame[%d] start", stream_no);
    pthread_mutex_lock(&player->mutex_queue);
    int ret = WAIT_FUNC_RET_OK;

    while (1) {
        if (player->flush_streams[stream_no]) {
            LOGI(3, "player_wait_for_frame[%d] flush_streams", stream_no);
            ret = WAIT_FUNC_RET_SKIP;
            break;
        }
        if (player->stop_streams[stream_no]) {
            LOGI(3, "player_wait_for_frame[%d] stop_streams", stream_no);
            ret = WAIT_FUNC_RET_SKIP;
            break;
        }
        if (player->pause) {
            pthread_cond_wait(&player->cond_queue, &player->mutex_queue);
            continue;
        }

        int64_t current_video_time = player_get_current_video_time(player);

        LOGI(8,
             "player_wait_for_frame[%d = %s] = (%f) - (%f)",
             stream_no,
             player->video_stream_no == stream_no ? "Video" : "Audio",
             stream_time/1000000.0,
             current_video_time/1000000.0);

        int64_t sleep_time = stream_time - current_video_time;

        LOGI(8,
             "player_wait_for_frame[%d] Waiting for frame: sleeping: %" SCNd64,
             stream_no, sleep_time);




        if (sleep_time < -300000ll || player->Test == 1 ) {     // Test had been added to reduce delay...we just trying to remove the few beginning frames.
            // 300 ms late                                      // Since we are only interested on what is going on now, we do not need those frames. In this
            //if (Test)                                         // way we can get better realtime camera view. We are doing this by skipping first few frames.
            //    LOGE(1, "player_wait test captured ");          // head = 100 does this trick. What we are doing here is to make the timestamp of the first frame
            player->Test = 0;                                   // as the start time.
            int64_t new_value = player->start_time - sleep_time;

            LOGI(4,
                 "player_wait_for_frame[%d] correcting %f to %f because late",
                 stream_no, (av_gettime() - player->start_time) / 1000000.0,
                 (av_gettime() - new_value) / 1000000.0);

            player->start_time = new_value;
            pthread_cond_broadcast(&player->cond_queue);

        }

        if (sleep_time <= MIN_SLEEP_TIME_US) {
            // We do not need to wait if time is slower then minimal sleep time
            break;
        }

        if (sleep_time > 500000ll) {
            // if sleep time is bigger then 500ms just sleep this 500ms
            // and check everything again
            sleep_time = 500000ll;
        }



        /*
        int timeout_ret = pthread_cond_timeout_np(&player->cond_queue,
                                                  &player->mutex_queue, sleep_time/1000ll);

                                                  */
        struct timespec ts;

        unsigned long time_in_ms = sleep_time/1000ll;

        unsigned long max_val = 1000000000ll;
        unsigned long ns;

        ms2ts(&ts, time_in_ms);

        struct timespec now;
        if(clock_gettime(CLOCK_REALTIME, &now) == 0) {

            ns = now.tv_nsec + (sleep_time * 1000)%max_val;
            ts.tv_sec = now.tv_sec;
            if (ns >= max_val) {
                ts.tv_sec++;
                ns = ns % max_val;
            }

            ts.tv_nsec = ns;

            int timeout_ret = pthread_cond_timedwait(&player->cond_queue,
                                                     &player->mutex_queue, &ts);


            if (timeout_ret == ETIMEDOUT) {
                // nothing special probably it is time ready to display
                // but for sure check everything again
                LOGI(9, "player_wait_for_frame[%d] timeout", stream_no);
            }

        }
        else
            LOGE(1, "error in getting current time");
    }

    // just go further
    LOGI(6, "player_wait_for_frame[%d] finish[%d]", stream_no, ret);
    pthread_mutex_unlock(&player->mutex_queue);
    return ret;
}

void player_decode_video_flush(struct DecoderData * decoder_data, JNIEnv * env) {
    struct Player *player = decoder_data->player;
    LOGI(2, "player_decode_video_flush flushing");
/*
#ifdef SUBTITLES
    if (player->subtitle_stream_no >= 0) {
		struct SubtitleElem * subtitle = NULL;
		while ((subtitle = queue_pop_start_already_locked_non_block(
				player->subtitles_queue)) != NULL) {
			avsubtitle_free(&subtitle->subtitle);
			queue_pop_finish_already_locked(player->subtitles_queue,
					&player->mutex_queue, &player->cond_queue);
		}
	}
#endif
 */
}


void * player_read_from_stream(void *data) {
    struct Player *player = (struct Player *) data;
    int err = ERROR_NO_ERROR;

    AVPacket packet, *pkt = &packet;
    int64_t seek_target;
    JNIEnv * env;
    Queue *queue;
    int seek_input_stream_number;
    AVStream * seek_input_stream;
    struct PacketData *packet_data;
    int to_write;
    int interrupt_ret;
    JavaVMAttachArgs thread_spec = { JNI_VERSION_1_4, "FFmpegReadFromStream",
                                     NULL };

    jint ret = (*player->get_javavm)->AttachCurrentThread(player->get_javavm,
                                                          &env, &thread_spec);
    if (ret) {
        err = ERROR_COULD_NOT_ATTACH_THREAD;
        goto end;
    }

    int head = 0;         // we are removeing first 100 frames to get better realtime camera view...

    LOGE(1, "skipping %d frames", head);
    player->Test = 1;
    for (;;) {
        int ret = av_read_frame(player->input_format_ctx, pkt);
        if (ret >= 0 && head > 0) {     // sipping first few frames
            head --;
            continue;
        }

        if (player->Test == 1) {
            player->error = 0;
            //callback(player, env);
        }


        if (ret < 0) {
            pthread_mutex_lock(&player->mutex_queue);
            LOGI(3, "player_read_from_stream stream end");
            queue = player->packets[player->video_stream_no];
            packet_data = queue_push_start_already_locked(queue,
                                                          &player->mutex_queue, &player->cond_queue, &to_write,
                                                          (QueueCheckFunc) player_read_from_stream_check_func, player,
                                                          (void **) &interrupt_ret);
            if (packet_data == NULL) {
                if (interrupt_ret == READ_FROM_STREAM_CHECK_MSG_STOP) {
                    LOGI(2, "player_read_from_stream queue interrupt stop");
                    goto exit_loop;
                } else if (interrupt_ret == READ_FROM_STREAM_CHECK_MSG_SEEK) {
                    LOGI(2, "player_read_from_stream queue interrupt seek");
                    goto seek_loop;
                } else {
                    assert(FALSE);
                }
            }
            packet_data->end_of_stream = TRUE;
            LOGI(3, "player_read_from_stream sending end_of_stream packet");
            queue_push_finish_already_locked(queue, &player->mutex_queue,
                                             &player->cond_queue, to_write);

            for (;;) {
                if (player->stop)
                    goto exit_loop;
                //if (player->seek_position != DO_NOT_SEEK)
                  //  goto seek_loop;
                pthread_cond_wait(&player->cond_queue, &player->mutex_queue);
            }
            pthread_mutex_unlock(&player->mutex_queue);
        }

        LOGI(8, "player_read_from_stream Read frame");
        pthread_mutex_lock(&player->mutex_queue);
        if (player->stop) {
            LOGI(4, "player_read_from_stream stopping");
            goto exit_loop;
        }
        //if (player->seek_position != DO_NOT_SEEK) {
          //  goto seek_loop;
        //}
        int stream_no;
        int caputre_streams_no = player->capture_streams_no;

        parse_frame: queue = NULL;
        LOGI(3, "player_read_from_stream looking for stream")
        for (stream_no = 0; stream_no < caputre_streams_no; ++stream_no) {
            if (packet.stream_index
                == player->input_stream_numbers[stream_no]) {
                queue = player->packets[stream_no];
                LOGI(3, "player_read_from_stream stream found [%d]", stream_no);
            }
        }

        if (queue == NULL) {
            LOGI(3, "player_read_from_stream stream not found (%d %d %d)", packet.stream_index, player->input_stream_numbers[0], caputre_streams_no);
            goto skip_loop;
        }

        push_start:
        LOGI(10, "player_read_from_stream waiting for queue");
        packet_data = queue_push_start_already_locked(queue,
                                                      &player->mutex_queue, &player->cond_queue, &to_write,
                                                      (QueueCheckFunc) player_read_from_stream_check_func, player,
                                                      (void **) &interrupt_ret);
        if (packet_data == NULL) {
            if (interrupt_ret == READ_FROM_STREAM_CHECK_MSG_STOP) {
                LOGI(2, "player_read_from_stream queue interrupt stop");
                goto exit_loop;
            } else if (interrupt_ret == READ_FROM_STREAM_CHECK_MSG_SEEK) {
                LOGI(2, "player_read_from_stream queue interrupt seek");
                goto seek_loop;
            } else {
                assert(FALSE);
            }
        }

        pthread_mutex_unlock(&player->mutex_queue);
        packet_data->end_of_stream = FALSE;
        *packet_data->packet = packet;

        if (av_dup_packet(packet_data->packet) < 0) {
            err = ERROR_WHILE_DUPLICATING_FRAME;
            pthread_mutex_lock(&player->mutex_queue);
            goto exit_loop;
        }

        queue_push_finish(queue, &player->mutex_queue, &player->cond_queue,
                          to_write);

        goto end_loop;

        exit_loop:
        LOGI(3, "player_read_from_stream stop");
        av_free_packet(pkt);

        //request stream to stop
        player_assign_to_no_boolean_array(player, player->stop_streams, TRUE);
        pthread_cond_broadcast(&player->cond_queue);

        // wait for all stream stop
        while (!player_if_all_no_array_elements_has_value(player,
                                                          player->stop_streams, FALSE)) {
            pthread_cond_wait(&player->cond_queue, &player->mutex_queue);
        }

        // flush internal buffers
        for (stream_no = 0; stream_no < caputre_streams_no; ++stream_no) {
            avcodec_flush_buffers(player->input_codec_ctxs[stream_no]);
        }

        pthread_mutex_unlock(&player->mutex_queue);
        goto detach_current_thread;

        seek_loop:
            /*
        // setting stream thet will be used as a base for seeking
        seek_input_stream_number =
                player->input_stream_numbers[player->video_stream_no];
        seek_input_stream = player->input_streams[player->video_stream_no];

        // getting seek target time in time_base value
        seek_target = av_rescale_q(
                player->seek_position, AV_TIME_BASE_Q,
                seek_input_stream->time_base);
        LOGI(3, "player_read_from_stream seeking to: "
                "%ds, time_base: %f", player->seek_position / 1000000.0, seek_target);

        // seeking
        if (av_seek_frame(player->input_format_ctx, seek_input_stream_number,
                          seek_target, 0) < 0) {
            // seeking error - trying to play movie without it
            LOGE(1, "Error while seeking");
            player->seek_position = DO_NOT_SEEK;
            pthread_cond_broadcast(&player->cond_queue);
            goto parse_frame;
        }

        LOGI(3, "player_read_from_stream seeking success");

        int64_t current_time = av_gettime();
        player->start_time = current_time - player->seek_position;
        player->pause_time = current_time;

        // request stream to flush
        player_assign_to_no_boolean_array(player, player->flush_streams, TRUE);
        if (player->audio_track != NULL) {
            LOGI(3, "player_read_from_stream flushing audio")
            // flush audio buffer
            (*env)->CallVoidMethod(env, player->audio_track,
                                   player->audio_track_flush_method);
            LOGI(3, "player_read_from_stream flushed audio");
        }

        pthread_cond_broadcast(&player->cond_queue);

        LOGI(3, "player_read_from_stream waiting for flush");

        // waiting for all stream flush
        while (!player_if_all_no_array_elements_has_value(player,
                                                          player->flush_streams, FALSE)) {
            pthread_cond_wait(&player->cond_queue, &player->mutex_queue);
        }

        LOGI(3, "player_read_from_stream flushing internal codec bffers");
        // flush internal buffers
        for (stream_no = 0; stream_no < caputre_streams_no; ++stream_no) {
            avcodec_flush_buffers(player->input_codec_ctxs[stream_no]);
        }

        // finishing seeking
        player->seek_position = DO_NOT_SEEK;
        pthread_cond_broadcast(&player->cond_queue);
        LOGI(3, "player_read_from_stream ending seek");

             */
        skip_loop: av_free_packet(pkt);
        pthread_mutex_unlock(&player->mutex_queue);


        end_loop: continue;

    }

    detach_current_thread: ret = (*player->get_javavm)->DetachCurrentThread(
            player->get_javavm);
    if (ret && !err)
        err = ERROR_COULD_NOT_DETACH_THREAD;

    end:

    // TODO do something with error valuse
    return NULL;
}

QueueCheckFuncRet player_read_from_stream_check_func(Queue *queue,
                                                     struct Player *player, int *ret) {
    if (player->stop) {
        *ret = READ_FROM_STREAM_CHECK_MSG_STOP;
        return QUEUE_CHECK_FUNC_RET_SKIP;
    }
    /*
    if (player->seek_position != DO_NOT_SEEK) {
        *ret = READ_FROM_STREAM_CHECK_MSG_SEEK;
        return QUEUE_CHECK_FUNC_RET_SKIP;
    }
     */
    return QUEUE_CHECK_FUNC_RET_TEST;
}


static void ms2ts(struct timespec *ts, unsigned long ms)
{
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000;
}