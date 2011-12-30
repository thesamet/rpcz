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
#include <iostream>
#include <utility>

#include "boost/bind.hpp"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/stubs/common.h"
#include "zmq.hpp"

#include "rpcz/callback.h"
#include "rpcz/connection_manager.h"
#include "rpcz/function_server.h"
#include "rpcz/logging.h"
#include "rpcz/macros.h"
#include "rpcz/rpc.h"
#include "rpcz/reactor.h"
#include "rpcz/service.h"
#include "rpcz/zmq_utils.h"
#include "rpcz/rpcz.pb.h"

namespace rpcz {

class ServerImpl {
 public:
  ServerImpl(zmq::socket_t* socket, FunctionServer* function_server,
             bool owns_socket);

  void Start();

  void RegisterService(Service *service);

 private:
  void HandleFunctionResponse(zmq::socket_t* fs_socket);

  void HandleRequestWorker(MessageVector* routes_,
                           MessageVector* data_,
                           FunctionServer::ReplyFunction reply);

  void HandleRequest(zmq::socket_t* fs_socket);

  zmq::socket_t* socket_;
  FunctionServer* function_server_;
  bool owns_socket_;
  typedef std::map<std::string, rpcz::Service*> ServiceMap;
  ServiceMap service_map_;
  DISALLOW_COPY_AND_ASSIGN(ServerImpl);
};

class ServerChannelImpl : public ServerChannel {
 public:
  ServerChannelImpl(MessageVector* routes,
                    zmq::message_t* request_id,
                    FunctionServer::ReplyFunction reply)
      : routes_(routes), request_id_(request_id),
        reply_(reply) { }

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
  scoped_ptr<MessageVector> routes_;
  scoped_ptr<zmq::message_t> request_id_;
  scoped_ptr<google::protobuf::Message> request_;
  FunctionServer::ReplyFunction reply_;

  // Sends the response back to a function server through the reply function.
  // Takes ownership of the provided payload message.
  void SendGenericResponse(const RpcResponseHeader& generic_rpc_response,
                           zmq::message_t* payload) {
    size_t msg_size = generic_rpc_response.ByteSize();
    zmq::message_t* zmq_response_message = new zmq::message_t(msg_size);
    CHECK(generic_rpc_response.SerializeToArray(
            zmq_response_message->data(),
            msg_size));

    routes_->push_back(request_id_.release());
    routes_->push_back(zmq_response_message);
    routes_->push_back(payload);
    reply_(routes_.get());
  }

  friend void ServerImpl::HandleRequestWorker(
      MessageVector* routes_, MessageVector* data_,
      FunctionServer::ReplyFunction reply);
};

ServerImpl::ServerImpl(zmq::socket_t* socket, FunctionServer* function_server,
             bool owns_socket)
  : socket_(socket), function_server_(function_server),
    owns_socket_(owns_socket) {}

void ServerImpl::Start() {
  // The reactor owns all sockets.
  Reactor reactor;
  zmq::socket_t* fs_socket = function_server_->GetConnectedSocket();
  reactor.AddSocket(socket_, NewPermanentCallback(
          this, &ServerImpl::HandleRequest, fs_socket));
  reactor.AddSocket(fs_socket,
                    NewPermanentCallback(
                        this, &ServerImpl::HandleFunctionResponse,
                        fs_socket));
  reactor.Loop();
  if (owns_socket_) {
    delete socket_;
  }
}

void ServerImpl::RegisterService(Service *service) {
  service_map_[service->GetDescriptor()->name()] = service;
}

void ServerImpl::HandleFunctionResponse(zmq::socket_t* fs_socket) {
  MessageVector data;
  CHECK(ReadMessageToVector(fs_socket, &data));
  data.erase_first();
  WriteVectorToSocket(socket_, data);
}

void ServerImpl::HandleRequestWorker(MessageVector* routes_,
                                     MessageVector* data_,
                                     FunctionServer::ReplyFunction reply) {
  // We are responsible to delete routes and data (and the pointers they
  // contain, so first wrap them in scoped_ptr's.
  scoped_ptr<ServerChannelImpl> channel(new ServerChannelImpl(
          routes_,
          data_->release(0),
          reply));

  scoped_ptr<MessageVector> data(data_);
  CHECK_EQ(3u, data->size());
  zmq::message_t& request = (*data)[1];
  zmq::message_t& payload = (*data)[2];

  RpcRequestHeader generic_rpc_request;
  if (!generic_rpc_request.ParseFromArray(request.data(), request.size())) {
    // Handle bad RPC.
    DLOG(INFO) << "Received bad header.";
    channel->SendError(application_error::INVALID_HEADER);
    return;
  };
  ServiceMap::const_iterator service_it = service_map_.find(
      generic_rpc_request.service());
  if (service_it == service_map_.end()) {
    // Handle invalid service.
    DLOG(INFO) << "Invalid service: " << generic_rpc_request.service();
    channel->SendError(application_error::NO_SUCH_SERVICE);
    return;
  }
  rpcz::Service* service = service_it->second;
  const ::google::protobuf::MethodDescriptor* descriptor =
      service->GetDescriptor()->FindMethodByName(
          generic_rpc_request.method());
  if (descriptor == NULL) {
    // Invalid method name
    DLOG(INFO) << "Invalid method name: " << generic_rpc_request.method();
    channel->SendError(application_error::NO_SUCH_METHOD);
    return;
  }
  channel->request_.reset(CHECK_NOTNULL(
          service->GetRequestPrototype(descriptor).New()));
  if (!channel->request_->ParseFromArray(payload.data(), payload.size())) {
    DLOG(INFO) << "Failed to parse request.";
    // Invalid proto;
    channel->SendError(application_error::INVALID_MESSAGE);
    return;
  }

  service->CallMethod(descriptor,
                      *channel->request_,
                      channel.release());
}

void ServerImpl::HandleRequest(zmq::socket_t* fs_socket) {
  scoped_ptr<MessageVector> routes(new MessageVector());
  scoped_ptr<MessageVector> data(new MessageVector());
  ReadMessageToVector(socket_, routes.get(), data.get());
  if (data->size() != 3) {
    DLOG(INFO) << "Dropping invalid requests.";
    return;
  }
  FunctionServer::AddFunction(
      fs_socket,
      boost::bind(&ServerImpl::HandleRequestWorker,
                  this, routes.release(), data.release(), _1));
}

Server::Server(zmq::socket_t* socket, EventManager* event_manager,
               bool owns_socket)
    : server_impl_(new ServerImpl(socket, event_manager->GetFunctionServer(),
                                  owns_socket)) {
}

Server::~Server() {
}

void Server::Start() {
  server_impl_->Start();
}

void Server::RegisterService(rpcz::Service *service) {
  server_impl_->RegisterService(service);
}
}  // namespace
