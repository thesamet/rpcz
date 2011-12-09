// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_CLIENT_REQUEST_H
#define ZRPC_CLIENT_REQUEST_H

#include <vector>
#include "zrpc/macros.h"
#include "zrpc/zmq_utils.h"

namespace zrpc {
struct ClientRequest {
  enum Status {
    INACTIVE = 0,
    ACTIVE = 1,
    DONE = 2,
    DEADLINE_EXCEEDED = 3,
  };
  Status status;
  MessageVector return_path;
  MessageVector result;
  Closure* closure;
  int64 deadline_ms;
  uint64 start_time;
};
}  // namespace zrpc
#endif
