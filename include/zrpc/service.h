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

#ifndef ZRPC_SERVICE_H
#define ZRPC_SERVICE_H

namespace google {
namespace protobuf {
class Message;
class MethodDescriptor;
class ServiceDescriptor;
}  // namespace protobuf
}  // namespace google

namespace zrpc {
class Closure;
class RPC;

class Service {
 public:
  Service() { };

  virtual const google::protobuf::ServiceDescriptor* GetDescriptor() = 0;

  virtual const google::protobuf::Message& GetRequestPrototype(
      const google::protobuf::MethodDescriptor*) const = 0;
  virtual const google::protobuf::Message& GetResponsePrototype(
      const google::protobuf::MethodDescriptor*) const = 0;

  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          RPC* rpc,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          Closure* done) = 0;
};
}  // namespace
#endif
