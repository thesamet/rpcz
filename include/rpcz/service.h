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

#ifndef RPCZ_SERVICE_H
#define RPCZ_SERVICE_H

#include <string>

namespace google {
namespace protobuf {
class Message;
class MethodDescriptor;
class ServiceDescriptor;
}  // namespace protobuf
}  // namespace google

namespace rpcz {
class Closure;
class RPC;
struct RpcRequestContext;

void FinalizeResponse(RpcRequestContext* context,
                      const google::protobuf::Message& response);

void FinalizeResponseWithError(RpcRequestContext* context,
                               int application_error,
                               const std::string& error_message);

template <typename T>
class Reply {
 public:
  explicit Reply(RpcRequestContext* context) :
      context_(context), replied_(false) {
  }

  ~Reply() {}
  
  void operator()(const T& reply) {
    assert(!replied_);
    FinalizeResponse(context_, reply);
    replied_ = true;
  }

  void Error(int application_error, const std::string& error_message="") {
    assert(!replied_);
    FinalizeResponseWithError(context_, application_error,
                              error_message);
    replied_ = true;
  }

 private:
  RpcRequestContext* context_;
  bool replied_;
};

class Service {
 public:
  Service() { };

  virtual const google::protobuf::ServiceDescriptor* GetDescriptor() = 0;

  virtual const google::protobuf::Message& GetRequestPrototype(
      const google::protobuf::MethodDescriptor*) const = 0;
  virtual const google::protobuf::Message& GetResponsePrototype(
      const google::protobuf::MethodDescriptor*) const = 0;

  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          const google::protobuf::Message& request,
                          RpcRequestContext* rpc_request_context) = 0;
};
}  // namespace
#endif
