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

#ifndef RPCZ_RPC_CHANNEL_IMPL_H
#define RPCZ_RPC_CHANNEL_IMPL_H

#include "rpcz/rpc_channel.h"

namespace rpcz {

class Connection;
class Closure;
class MessageVector;
struct RpcResponseContext;

class RpcChannelImpl: public RpcChannel {
 public:
  RpcChannelImpl(Connection* connection, bool owns_connection=false);

  virtual ~RpcChannelImpl();

  virtual void CallMethod(const std::string& service_name,
                          const google::protobuf::MethodDescriptor* method,
                          const google::protobuf::Message& request,
                          google::protobuf::Message* response, RPC* rpc,
                          Closure* done);

  virtual void CallMethod0(
      const std::string& service_name,
      const std::string& method_name,
      const std::string& request,
      std::string* response, RPC* rpc, Closure* done);

 private:
  virtual void HandleClientResponse(MessageVector* request,
                                    RpcResponseContext *response_context);

  void CallMethodFull(
    const std::string& service_name,
    const std::string& method_name,
    const ::google::protobuf::Message* request_msg,
    const std::string& request,
    ::google::protobuf::Message* response_msg,
    std::string* response_str,
    RPC* rpc,
    Closure* done);

  Connection* connection_;
  bool owns_connection_;
};
} // namespace rpcz
#endif /* RPCZ_SIMPLE_RPC_CHANNEL_IMPL_H_ */
