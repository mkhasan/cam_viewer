package com.railbot.usrc.mediaplayer;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.AsyncTask;
import android.util.Log;
import android.view.Surface;

import java.util.Map;

/**
 * Created by usrc on 16. 12. 12.
 */

public class VideoPlayer {

    static {

        System.loadLibrary("yuv");
//        System.loadLibrary("ffmpeg");

        System.loadLibrary("avutil");
        System.loadLibrary("swresample");
        System.loadLibrary("avcodec");
        System.loadLibrary("avformat");
        System.loadLibrary("swscale");
        System.loadLibrary("avfilter");
        System.loadLibrary("avdevice");

        System.loadLibrary("native-lib");


    }


    private static final String TAG 	 = "VideoPlayer";



    private static class StopTask extends AsyncTask<Void, Void, Void> {

        private final VideoPlayer player;

        public StopTask(VideoPlayer player) {
            this.player = player;

        }


        @Override
        protected Void doInBackground(Void... params) {

            if(player.mNativePlayer != 0)
                player.stopNative();
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {

            if (player.mpegListener != null) {
                Log.e(TAG, "Stoping almost done");
                player.mpegListener.onFFStop();
            }
        }


    }

    private static class CommErrorTask extends AsyncTask<Void, Void, Void> {

        private final VideoPlayer player;
        private final int errorCode;

        public CommErrorTask(VideoPlayer player, int errorCode) {
            this.player = player;
            this.errorCode = errorCode;

        }


        @Override
        protected Void doInBackground(Void... params) {


            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            if (player.mpegListener != null) {
                FFError err = null;
                if (errorCode != 0)
                    err = new FFError(errorCode);
                player.mpegListener.onFFError(err);
            }
        }


    }
    private static class SetDataSourceTaskResult {
        FFError error;
        StreamInfo[] streams;
    }

    private static class SetDataSourceTask extends
            AsyncTask<Object, Void, SetDataSourceTaskResult> {

        private final VideoPlayer player;

        public SetDataSourceTask(VideoPlayer player) {
            this.player = player;
        }

        @Override
        protected SetDataSourceTaskResult doInBackground(Object... params) {
            String url = (String) params[0];
            @SuppressWarnings("unchecked")
            Map<String, String> map = (Map<String, String>) params[1];
            Integer videoStream = (Integer) params[2];
            Integer audioStream = (Integer) params[3];
            Integer subtitleStream = (Integer) params[4];

            int videoStreamNo = videoStream == null ? -1 : videoStream.intValue();
            int audioStreamNo = audioStream == null ? -1 : audioStream.intValue();
            int subtitleStreamNo = subtitleStream == null ? -1 : subtitleStream.intValue();


            int err = player.setDataSourceNative(url);//, map, videoStreamNo, audioStreamNo, subtitleStreamNo);

            SetDataSourceTaskResult result = new SetDataSourceTaskResult();


            if (err < 0) {
                result.error = new FFError(err);
                result.streams = null;
            } else {
                result.error = null;
                result.streams = player.getStreamsInfo();
            }

            Log.e(TAG, "SetDataSourceTaskResult done");
            return result;
        }

        @Override
        protected void onPostExecute(SetDataSourceTaskResult result) {
            Log.e(TAG, "sending result to " + (player.mpegListener == null ? "null" : "not null"));
            if (player.mpegListener != null)
                player.mpegListener.onFFDataSourceLoaded(result.error,
                       result.streams);
        }

    }


    public static final int UNKNOWN_STREAM = -1;
    public static final int NO_STREAM = -2;
    private FFListener mpegListener = null;
    private final RenderedFrame mRenderedFrame = new RenderedFrame();

    private long mNativePlayer;
    private final Activity activity;


    private Runnable updateTimeRunnable = new Runnable() {

        @Override
        public void run() {

            if (mpegListener != null) {
                mpegListener.onFFUpdateTime(mCurrentTimeUs,
                        mVideoDurationUs, mIsFinished);
            }

        }

    };

    private long mCurrentTimeUs;
    private long mVideoDurationUs;
    //private FFmpegStreamInfo[] mStreamsInfos = null;
    private boolean mIsFinished = false;

    static class RenderedFrame {
        public Bitmap bitmap;
        public int height;
        public int width;
    }

    public VideoPlayer(VideoDisplay videoView, Activity activity) {


        this.activity = activity;
        mNativePlayer = 0;

        int error = initNative();
        if (error != 0)
            throw new RuntimeException(String.format(
                    "Could not initialize player: %d", error));

        videoView.setMpegPlayer(this);

    }

    public void Show() {
        Test();
    }

    public void stop() {
        Log.e(TAG, "going to stop");
        new StopTask(this).execute();
    }


    @Override
    protected void finalize() throws Throwable {
        /*
        Log.e("dealloc", "deallocating");
        deallocNative();
        */
        super.finalize();

    }

    public long NativePlayer() {
        return mNativePlayer;
    }


    public native int initNative();
    private native int deallocNative();
    public native void render(Surface surface);
    public native int setDataSourceNative(String url);
    private native void stopNative();
    public native void renderFrameStart();
    public native void renderFrameStop();
    private native void Test();

    public void setDataSource(String url, Map<String, String> dictionary,
                              int videoStream, int audioStream, int subtitlesStream) {
        new SetDataSourceTask(this).execute(url, dictionary,
                Integer.valueOf(videoStream), Integer.valueOf(audioStream),
                Integer.valueOf(subtitlesStream));
    }

    public void setListener(FFListener mpegListener) {
        this.mpegListener = mpegListener;

        Log.e(TAG, "mpegListener is "+(mpegListener == null ? "null" : "not null"));


    }
    private StreamInfo[] mStreamsInfos = null;
    protected StreamInfo[] getStreamsInfo() {
        return mStreamsInfos;
    }

    private void callbackError(int i) {


        //mpegListener.onFFError();
        new CommErrorTask(this, i).execute();
        Log.e(TAG, "From callback: " + Integer.toString(i));
    }

    public void DeallocatePlayer() {
        deallocNative();
        mNativePlayer = 0;
    }

    private void onUpdateTime(long currentUs, long maxUs, boolean isFinished) {

        this.mCurrentTimeUs = currentUs;
        this.mVideoDurationUs = maxUs;
        this.mIsFinished  = isFinished;
        activity.runOnUiThread(updateTimeRunnable);
    }


}

