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

#include "rpcz/server.h"
#include <signal.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <functional>
#include <iostream>
#include <utility>

#include "boost/bind.hpp"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/stubs/common.h"
#include "zmq.hpp"

#include "rpcz/callback.h"
#include "rpcz/connection_manager.h"
#include "rpcz/logging.h"
#include "rpcz/macros.h"
#include "rpcz/rpc.h"
#include "rpcz/reactor.h"
#include "rpcz/service.h"
#include "rpcz/zmq_utils.h"
#include "rpcz/rpcz.pb.h"

namespace rpcz {

class ServerChannelImpl : public ServerChannel {
 public:
  ServerChannelImpl(const ClientConnection& connection)
      : connection_(connection) {
      }

  virtual void Send(const google::protobuf::Message& response) {
    RpcResponseHeader generic_rpc_response;
    int msg_size = response.ByteSize();
    scoped_ptr<zmq::message_t> payload(new zmq::message_t(msg_size));
    if (!response.SerializeToArray(payload->data(), msg_size)) {
      throw InvalidMessageError("Invalid response message");
    }
    SendGenericResponse(generic_rpc_response,
                        payload.release());
  }

  virtual void Send0(const std::string& response) {
    RpcResponseHeader generic_rpc_response;
    SendGenericResponse(generic_rpc_response,
                        StringToMessage(response));
  }

  virtual void SendError(int application_error,
                         const std::string& error_message="") {
    RpcResponseHeader generic_rpc_response;
    zmq::message_t* payload = new zmq::message_t();
    generic_rpc_response.set_status(status::APPLICATION_ERROR);
    generic_rpc_response.set_application_error(application_error);
    if (!error_message.empty()) {
      generic_rpc_response.set_error(error_message);
    }
    SendGenericResponse(generic_rpc_response,
                        payload);
  }

 private:
  ClientConnection connection_;
  scoped_ptr<google::protobuf::Message> request_;

  // Sends the response back to a function server through the reply function.
  // Takes ownership of the provided payload message.
  void SendGenericResponse(const RpcResponseHeader& generic_rpc_response,
                           zmq::message_t* payload) {
    size_t msg_size = generic_rpc_response.ByteSize();
    zmq::message_t* zmq_response_message = new zmq::message_t(msg_size);
    CHECK(generic_rpc_response.SerializeToArray(
            zmq_response_message->data(),
            msg_size));

    MessageVector v;
    v.push_back(zmq_response_message);
    v.push_back(payload);
    connection_.Reply(&v);
  }

  friend class ProtoRpcService;
};

class ProtoRpcService : public RpcService {
 public:
  explicit ProtoRpcService(Service* service) : service_(service) {
  }

  virtual void DispatchRequest(const std::string& method,
                               const void* payload, size_t payload_len,
                               ServerChannel* channel_) {
    scoped_ptr<ServerChannelImpl> channel(
        static_cast<ServerChannelImpl*>(channel_));

    const ::google::protobuf::MethodDescriptor* descriptor =
        service_->GetDescriptor()->FindMethodByName(
            method);
    if (descriptor == NULL) {
      // Invalid method name
      DLOG(INFO) << "Invalid method name: " << method,
      channel->SendError(application_error::NO_SUCH_METHOD);
      return;
    }
    channel->request_.reset(CHECK_NOTNULL(
            service_->GetRequestPrototype(descriptor).New()));
    if (!channel->request_->ParseFromArray(payload, payload_len)) {
      DLOG(INFO) << "Failed to parse request.";
      // Invalid proto;
      channel->SendError(application_error::INVALID_MESSAGE);
      return;
    }
    ServerChannelImpl* channel_ptr = channel.release();
    service_->CallMethod(descriptor,
                         *channel_ptr->request_,
                         channel_ptr);
  }

 private:
  scoped_ptr<Service> service_;
};

Server::Server(ConnectionManager* connection_manager)
  : connection_manager_(connection_manager) {
}

Server::~Server() {
  DeleteContainerSecondPointer(service_map_.begin(),
                               service_map_.end());
}

void Server::RegisterService(rpcz::Service *service) {
  RegisterService(service,
                  service->GetDescriptor()->name());
}

void Server::RegisterService(rpcz::Service *service, const std::string& name) {
  RegisterService(new ProtoRpcService(service),
                  name);
}

void Server::RegisterService(rpcz::RpcService *rpc_service,
                             const std::string& name) {
  service_map_[name] = rpc_service;
}

void Server::Bind(const std::string& endpoint) {
  ConnectionManager::ServerFunction f = boost::bind(
      &Server::HandleRequest, this, _1, _2);
  connection_manager_->Bind(endpoint, f);
}

void Server::HandleRequest(const ClientConnection& connection,
                           MessageIterator& iter) {
  if (!iter.has_more()) {
    return;
  }
  zmq_message request = iter.next();
  if (!iter.has_more()) {
    return;
  }
  zmq_message payload = iter.next();
  if (iter.has_more()) {
    return;
  }
  scoped_ptr<ServerChannel> channel(new ServerChannelImpl(connection));

  RpcRequestHeader rpc_request_header;
  if (!rpc_request_header.ParseFromArray(request.data(), request.size())) {
    // Handle bad RPC.
    DLOG(INFO) << "Received bad header.";
    channel->SendError(application_error::INVALID_HEADER);
    return;
  };
  RpcServiceMap::const_iterator service_it = service_map_.find(
      rpc_request_header.service());
  if (service_it == service_map_.end()) {
    // Handle invalid service.
    DLOG(INFO) << "Invalid service: " << rpc_request_header.service();
    channel->SendError(application_error::NO_SUCH_SERVICE);
    return;
  }
  rpcz::RpcService* service = service_it->second;
  service->DispatchRequest(rpc_request_header.method(), payload.data(),
                           payload.size(),
                           channel.release());
}
}  // namespace
