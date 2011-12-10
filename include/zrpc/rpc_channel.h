// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_RPC_CHANNEL_H
#define ZRPC_RPC_CHANNEL_H

#include <string>
#include <set>

#include "google/protobuf/stubs/common.h"
#include "zrpc/macros.h"

namespace google {
namespace protobuf {
class Message;
class MethodDescriptor;
}  // namespace protobuf
}  // namespace google

namespace zrpc {
class RPC;

class RpcChannel {
 public:
  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          RPC* rpc,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          Closure* done) = 0;

  // DO NOT USE: this method exists only for language bindings and may be
  // removed.
  virtual void CallMethod0(const std::string& service_name,
                           const std::string& method_name,
                           RPC* rpc,
                           const std::string& request,
                           std::string* response,
                           google::protobuf::Closure* done) = 0;

  virtual ~RpcChannel() {};
};
}  // namespace
#endif
