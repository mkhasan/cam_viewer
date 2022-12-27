package com.railbot.usrc.mediaplayer;

/**
 * Created by usrc on 16. 12. 28.
 */


public interface FFListener {
    void onFFDataSourceLoaded(FFError err, StreamInfo[] streams);

    void onFFResume(NotPlayingException result);

    void onFFPause(NotPlayingException err);

    void onFFStop();

    void onFFUpdateTime(long mCurrentTimeUs, long mVideoDurationUs, boolean isFinished);

    void onFFSeeked(NotPlayingException result);

    void onFFError(FFError err);

}