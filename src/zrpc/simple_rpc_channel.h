// Copyright 2011 Google Inc. All Rights Reserved.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: nadavs@google.com <Nadav Samet>

#ifndef ZRPC_SIMPLE_RPC_CHANNEL_H
#define ZRPC_SIMPLE_RPC_CHANNEL_H

#include "zrpc/rpc_channel.h"

namespace zmq {
class message_t; 
}  // namespace zmq

namespace zrpc {

class Connection;
class ClientRequest;
class Closure;
class ConnectionManager;
template <typename T>
class PointerVector;
typedef PointerVector<zmq::message_t> MessageVector;
struct RpcResponseContext;

class SimpleRpcChannel: public RpcChannel {
 public:
  SimpleRpcChannel(Connection* connection);

  virtual ~SimpleRpcChannel();

  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          RPC* rpc, const google::protobuf::Message* request,
                          google::protobuf::Message* response, Closure* done);

  virtual void CallMethod0(
      const std::string& service_name,
      const std::string& method_name, RPC* rpc,
      const std::string& request,
      std::string* response, Closure* done);

 private:
  virtual void HandleClientResponse(MessageVector* request,
                                    RpcResponseContext *response_context);

  void CallMethodFull(
    const std::string& service_name,
    const std::string& method_name,
    RPC* rpc,
    const std::string& request,
    std::string* response_str,
    ::google::protobuf::Message* response_msg,
    Closure* done);

  Connection* connection_;
};
} // namespace zrpc
#endif /* ZRPC_SIMPLE_RPC_CHANNEL_H_ */
