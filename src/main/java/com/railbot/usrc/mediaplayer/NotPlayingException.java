package com.railbot.usrc.mediaplayer;

/**
 * Created by usrc on 16. 12. 12.
 */

public class NotPlayingException extends Exception {
    private static final long serialVersionUID = 1L;

    public NotPlayingException(String detailMessage, Throwable throwable) {
        super(detailMessage, throwable);
    }

    public NotPlayingException(String detailMessage) {
        super(detailMessage);
    }

    public NotPlayingException() {
        super();
    }

}
