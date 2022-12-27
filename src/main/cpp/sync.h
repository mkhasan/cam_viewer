//
// Created by usrc on 16. 12. 14.
//

#ifndef MEDIAPLAYER_SYNC_H
#define MEDIAPLAYER_SYNC_H

#include <stdint.h>

enum WaitFuncRet {
    WAIT_FUNC_RET_OK = 0,
    WAIT_FUNC_RET_SKIP = 1,
};

typedef enum WaitFuncRet (WaitFunc) (void *data , int64_t time, int stream_no);

#endif //MEDIAPLAYER_SYNC_H
