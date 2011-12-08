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
    OK = 0,
    FAILED = 1
  };
  Status status;
  MessageVector return_path;
  MessageVector result;
  Closure* closure;
};
}  // namespace zrpc
#endif
