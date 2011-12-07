// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_CLIENT_REQUEST_H
#define ZRPC_CLIENT_REQUEST_H

#include <vector>
#include "zrpc/macros.h"

namespace zmq {
class message_t; 
}  // namespace zmq

namespace zrpc {
struct ClientRequest {
  enum Status {
    OK = 0,
    FAILED = 1
  };
  Status status;
  std::vector<zmq::message_t*> return_path;
  std::vector<zmq::message_t*> result;
  Closure* closure;
};
}  // namespace zrpc
#endif
