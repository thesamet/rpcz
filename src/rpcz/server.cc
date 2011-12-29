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
struct RpcRequestContext {
  RPC rpc;
  scoped_ptr<MessageVector> routes;
  scoped_ptr<zmq::message_t> request_id;

  scoped_ptr<google::protobuf::Message> request;
  FunctionServer::ReplyFunction reply;
};

namespace {
// Sends the response back to a function server through the reply function.
// Takes ownership of the provided payload message.
void SendGenericResponse(RpcRequestContext& context,
                         const RpcResponseHeader& generic_rpc_response,
                         zmq::message_t* payload) {
  size_t msg_size = generic_rpc_response.ByteSize();
  zmq::message_t* zmq_response_message = new zmq::message_t(msg_size);
  CHECK(generic_rpc_response.SerializeToArray(
      zmq_response_message->data(),
      msg_size));

  context.routes->push_back(context.request_id.release());
  context.routes->push_back(zmq_response_message);
  context.routes->push_back(payload);
  context.reply(context.routes.get());
}

void ReplyWithAppError(RpcRequestContext& context,
                       int application_error,
                       const std::string& error="") {
  RpcResponseHeader response;
  response.set_status(status::APPLICATION_ERROR);
  response.set_application_error(application_error);
  if (!error.empty()) {
    response.set_error(error);
  }
  SendGenericResponse(context,
                      response, new zmq::message_t());
}
}  // unnamed namespace 

void FinalizeResponse(RpcRequestContext* context,
                      const google::protobuf::Message& response) {
  RpcResponseHeader generic_rpc_response;
  int msg_size = response.ByteSize();
  scoped_ptr<zmq::message_t> payload(new zmq::message_t(msg_size));
  if (!response.SerializeToArray(payload->data(), msg_size)) {
    throw InvalidMessageError("Invalid response message");
  }
  SendGenericResponse(*context,
                      generic_rpc_response,
                      payload.release());
  delete context;
}

void FinalizeResponseWithError(RpcRequestContext* context,
                               int application_error,
                               const std::string& error_message) {
  RpcResponseHeader generic_rpc_response;
  zmq::message_t* payload = new zmq::message_t();
  generic_rpc_response.set_status(status::APPLICATION_ERROR);
  generic_rpc_response.set_application_error(application_error);
  if (!error_message.empty()) {
    generic_rpc_response.set_error(error_message);
  }
  SendGenericResponse(*context,
                      generic_rpc_response,
                      payload);
  delete context;
}

class ServerImpl {
 public:
  ServerImpl(zmq::socket_t* socket, FunctionServer* function_server,
             bool owns_socket)
    : socket_(socket), function_server_(function_server),
      owns_socket_(owns_socket) {}

  void Start() {
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

  void RegisterService(Service *service) {
    service_map_[service->GetDescriptor()->name()] = service;
  }

 private:
  void HandleFunctionResponse(zmq::socket_t* fs_socket) {
    MessageVector data;
    CHECK(ReadMessageToVector(fs_socket, &data));
    data.erase_first();
    WriteVectorToSocket(socket_, data);
  }

  void HandleRequestWorker(MessageVector* routes_,
                           MessageVector* data_,
                           FunctionServer::ReplyFunction reply) {
    // We are responsible to delete routes and data (and the pointers they
    // contain, so first wrap them in scoped_ptr's.
    scoped_ptr<RpcRequestContext> context(CHECK_NOTNULL(new RpcRequestContext));
    context->routes.reset(routes_);
    context->request_id.reset(data_->release(0));
    context->reply = reply;

    scoped_ptr<MessageVector> data(data_);
    CHECK_EQ(3u, data->size());
    zmq::message_t& request = (*data)[1];
    zmq::message_t& payload = (*data)[2];

    RpcRequestHeader generic_rpc_request;
    if (!generic_rpc_request.ParseFromArray(request.data(), request.size())) {
      // Handle bad RPC.
      DLOG(INFO) << "Received bad header.";
      ReplyWithAppError(*context,
                        application_error::INVALID_HEADER);
      return;
    };
    ServiceMap::const_iterator service_it = service_map_.find(
        generic_rpc_request.service());
    if (service_it == service_map_.end()) {
      // Handle invalid service.
      DLOG(INFO) << "Invalid service: " << generic_rpc_request.service();
      ReplyWithAppError(*context, application_error::NO_SUCH_SERVICE);
      return;
    }
    rpcz::Service* service = service_it->second;
    const ::google::protobuf::MethodDescriptor* descriptor =
        service->GetDescriptor()->FindMethodByName(
            generic_rpc_request.method());
    if (descriptor == NULL) {
      // Invalid method name
      DLOG(INFO) << "Invalid method name: " << generic_rpc_request.method();
      ReplyWithAppError(*context, application_error::NO_SUCH_METHOD);
      return;
    }
    context->request.reset(CHECK_NOTNULL(
        service->GetRequestPrototype(descriptor).New()));
    if (!context->request->ParseFromArray(payload.data(), payload.size())) {
      DLOG(INFO) << "Failed to parse request.";
      // Invalid proto;
      ReplyWithAppError(*context,
                        application_error::INVALID_MESSAGE);
      return;
    }

    context->rpc.SetStatus(status::OK);
    service->CallMethod(descriptor,
                        *context->request,
                        context.release());
  }

  void HandleRequest(zmq::socket_t* fs_socket) {
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

  zmq::socket_t* socket_;
  FunctionServer* function_server_;
  bool owns_socket_;
  typedef std::map<std::string, rpcz::Service*> ServiceMap;
  ServiceMap service_map_;
  DISALLOW_COPY_AND_ASSIGN(ServerImpl);
};

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
