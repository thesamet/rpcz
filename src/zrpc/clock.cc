// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "clock.h"
#include <sys/time.h>

namespace zrpc {

uint64 zclock_time(void) {
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

}  // namespace zrpc
