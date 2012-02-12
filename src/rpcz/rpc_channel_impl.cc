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

#include <google/protobuf/descriptor.h>
#include <zmq.hpp>
#include "rpcz/callback.h"
#include "rpcz/connection_manager.h"
#include "rpcz/logging.h"
#include "rpcz/remote_response.h"
#include "rpcz/rpc.h"
#include "rpcz/rpc_channel_impl.h"
#include "rpcz/sync_event.h"

namespace rpcz {

RpcChannel* RpcChannel::Create(Connection connection) {
  return new RpcChannelImpl(connection);
}

RpcChannelImpl::RpcChannelImpl(Connection connection)
    : connection_(connection) {
}

RpcChannelImpl::~RpcChannelImpl() {
}

struct RpcResponseContext {
  RPC* rpc;
  ::google::protobuf::Message* response_msg;
  std::string* response_str;
  Closure* user_closure;
};

void RpcChannelImpl::CallMethodFull(
    const std::string& service_name,
    const std::string& method_name,
    const ::google::protobuf::Message* request_msg,
    const std::string& request,
    ::google::protobuf::Message* response_msg,
    std::string* response_str,
    RPC* rpc,
    Closure* done) {
  CHECK_EQ(rpc->GetStatus(), status::INACTIVE);
  RpcRequestHeader generic_request;
  generic_request.set_service(service_name);
  generic_request.set_method(method_name);

  size_t msg_size = generic_request.ByteSize();
  scoped_ptr<zmq::message_t> msg_out(new zmq::message_t(msg_size));
  CHECK(generic_request.SerializeToArray(msg_out->data(), msg_size));

  scoped_ptr<zmq::message_t> payload_out;
  if (request_msg != NULL) {
    size_t bytes = request_msg->ByteSize();
    payload_out.reset(new zmq::message_t(bytes));
    if (!request_msg->SerializeToArray(payload_out->data(),
                                       bytes)) {
      throw new InvalidMessageError("Request serialization failed.");
    }
  } else {
    payload_out.reset(StringToMessage(request));
  }

  MessageVector msg_vector;
  msg_vector.push_back(msg_out.release());
  msg_vector.push_back(payload_out.release());

  RpcResponseContext response_context;
  response_context.rpc = rpc;
  response_context.user_closure = done;
  response_context.response_str = response_str;
  response_context.response_msg = response_msg;
  rpc->SetStatus(status::ACTIVE);

  connection_.SendRequest(
      msg_vector,
      rpc->GetDeadlineMs(),
      bind(&RpcChannelImpl::HandleClientResponse, this,
           response_context, _1, _2));
}

void RpcChannelImpl::CallMethod0(const std::string& service_name,
                                const std::string& method_name,
                                const std::string& request,
                                std::string* response,
                                RPC* rpc,
                                Closure* done) {
  CallMethodFull(service_name,
                 method_name,
                 NULL,
                 request,
                 NULL,
                 response,
                 rpc,
                 done);
}

void RpcChannelImpl::CallMethod(
    const std::string& service_name,
    const google::protobuf::MethodDescriptor* method,
    const google::protobuf::Message& request,
    google::protobuf::Message* response,
    RPC* rpc,
    Closure* done) {
  CallMethodFull(service_name,
                 method->name(),
                 &request,
                 "",
                 response,
                 NULL,
                 rpc,
                 done);
}

void RpcChannelImpl::HandleClientResponse(
    RpcResponseContext response_context, ConnectionManager::Status status,
    MessageIterator& iter) {
  switch (status) {
    case ConnectionManager::DEADLINE_EXCEEDED:
      response_context.rpc->SetStatus(
          status::DEADLINE_EXCEEDED);
      break;
    case ConnectionManager::DONE: {
        if (!iter.has_more()) {
          response_context.rpc->SetFailed(application_error::INVALID_MESSAGE,
                                           "");
          break;
        }
        RpcResponseHeader generic_response;
        zmq::message_t& msg_in = iter.next();
        if (!generic_response.ParseFromArray(msg_in.data(), msg_in.size())) {
          response_context.rpc->SetFailed(application_error::INVALID_MESSAGE,
                                           "");
          break;
        }
        if (generic_response.status() != status::OK) {
          response_context.rpc->SetFailed(generic_response.application_error(),
                                           generic_response.error());
        } else {
          response_context.rpc->SetStatus(status::OK);
          zmq::message_t& payload = iter.next();
          if (response_context.response_msg) {
            if (!response_context.response_msg->ParseFromArray(
                    payload.data(),
                    payload.size())) {
              response_context.rpc->SetFailed(application_error::INVALID_MESSAGE,
                                              "");
              break;
            }
          } else if (response_context.response_str) {
            response_context.response_str->assign(
                static_cast<char*>(
                    payload.data()),
                payload.size());
          }
        }
      }
      break;
    case ConnectionManager::ACTIVE:
    case ConnectionManager::INACTIVE:
    default:
      CHECK(false) << "Unexpected status: "
                   << status;
  }
  // We call Signal() before we execute closure since the closure may delete
  // the RPC object (which contains the sync_event).
  response_context.rpc->sync_event_->Signal();
  if (response_context.user_closure) {
    response_context.user_closure->Run();
  }
}
}  // namespace rpcz
