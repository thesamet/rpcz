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

#include <assert.h>
#include <string>

namespace google {
namespace protobuf {
class Message;
class MethodDescriptor;
class ServiceDescriptor;
}  // namespace protobuf
}  // namespace google

namespace rpcz {

class ServerChannel {
 public:
  virtual void Send(const google::protobuf::Message& response) = 0;
  virtual void SendError(int application_error,
                         const std::string& error_message = "") = 0;
  virtual ~ServerChannel() {}

  // Hack to allow language bindings to do the serialization at their
  // end. Do not use directly.
  virtual void Send0(const std::string& response) = 0;
};

template <typename MessageType>
class Reply {
 public:
  explicit Reply(ServerChannel* channel) :
      channel_(channel), replied_(false) {
  }

  ~Reply() { }

  void Send(const MessageType& response) {
    assert(!replied_);
    channel_->Send(response);
    delete channel_;
    replied_ = true;
  }

  void Error(int application_error, const std::string& error_message="") {
    assert(!replied_);
    channel_->SendError(application_error, error_message);
    delete channel_;
    replied_ = true;
  }

 private:
  ServerChannel* channel_;
  bool replied_;
};

class Service {
 public:
  Service() { };

  virtual ~Service() {};

  virtual const google::protobuf::ServiceDescriptor* GetDescriptor() = 0;

  virtual const google::protobuf::Message& GetRequestPrototype(
      const google::protobuf::MethodDescriptor*) const = 0;
  virtual const google::protobuf::Message& GetResponsePrototype(
      const google::protobuf::MethodDescriptor*) const = 0;

  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          const google::protobuf::Message& request,
                          ServerChannel* server_channel) = 0;
};
}  // namespace
#endif
