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

#include "zrpc/server.h"
#include <signal.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <iostream>
#include <utility>

#include "boost/bind.hpp"
#include "glog/logging.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/stubs/common.h"
#include "zmq.hpp"

#include "zrpc/callback.h"
#include "zrpc/connection_manager.h"
#include "zrpc/function_server.h"
#include "zrpc/macros.h"
#include "zrpc/rpc.h"
#include "zrpc/reactor.h"
#include "zrpc/service.h"
#include "zrpc/string_piece.h"
#include "zrpc/zmq_utils.h"
#include "zrpc/zrpc.pb.h"

namespace zrpc {
namespace {
struct RPCRequestContext {
  RPC rpc;
  scoped_ptr<MessageVector> routes;
  scoped_ptr<zmq::message_t> request_id;

  scoped_ptr<google::protobuf::Message> request;
  scoped_ptr<google::protobuf::Message> response;
};

// Sends the response back to a function server through the reply function.
// Takes ownership of the provided payload message.
void SendGenericResponse(RPCRequestContext& context,
                         const GenericRPCResponse& generic_rpc_response,
                         zmq::message_t* payload,
                         FunctionServer::ReplyFunction reply) {
  size_t msg_size = generic_rpc_response.ByteSize();
  zmq::message_t* zmq_response_message = new zmq::message_t(msg_size);
  CHECK(generic_rpc_response.SerializeToArray(
      zmq_response_message->data(),
      msg_size));

  context.routes->push_back(context.request_id.release());
  context.routes->push_back(zmq_response_message);
  context.routes->push_back(payload);
  reply(context.routes.get());
}

void ReplyWithAppError(FunctionServer::ReplyFunction reply,
                       RPCRequestContext& context,
                       int application_error,
                       const std::string& error="") {
  GenericRPCResponse response;
  response.set_status(GenericRPCResponse::APPLICATION_ERROR);
  response.set_application_error(application_error);
  if (!error.empty()) {
    response.set_error(error);
  }
  SendGenericResponse(context,
                      response, new zmq::message_t(), reply);
}

void FinalizeResponse(
    RPCRequestContext *context,
    FunctionServer::ReplyFunction reply) {
  GenericRPCResponse generic_rpc_response;
  int msg_size = context->response->ByteSize();
  zmq::message_t* payload = NULL;
  if (context->rpc.OK()) {
    payload = new zmq::message_t(msg_size);
    CHECK(context->response->SerializeToArray(payload->data(), msg_size));
  } else {
    generic_rpc_response.set_status(
        context->rpc.GetStatus());
    generic_rpc_response.set_application_error(
        context->rpc.GetApplicationError());
    std::string error_message(context->rpc.GetErrorMessage());
    if (!error_message.empty()) {
      generic_rpc_response.set_error(error_message);
    }
    payload = new zmq::message_t;
  }
  SendGenericResponse(*context,
                      generic_rpc_response,
                      payload, reply); 
  delete context;
}
}  // unnamed namespace 

class ServerImpl {
 public:
  ServerImpl(zmq::socket_t* socket, FunctionServer* function_server)
    : socket_(socket), function_server_(function_server) {}

  void Start() {
    InstallSignalHandler();
    // The reactor owns all sockets.
    Reactor reactor;
    zmq::socket_t* fs_socket = function_server_->GetConnectedSocket();
    reactor.AddSocket(socket_, NewPermanentCallback(
          this, &ServerImpl::HandleRequest, fs_socket));
    reactor.AddSocket(fs_socket,
                      NewPermanentCallback(
                          this, &ServerImpl::HandleFunctionResponse,
                          fs_socket));
    reactor.LoopUntil(NULL);
    LOG(INFO) << "Server shutdown.";
  }

  void RegisterService(Service *service) {
    VLOG(2) << "Registering service '" << service->GetDescriptor()->name()
            << "'";
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
    scoped_ptr<RPCRequestContext> context(CHECK_NOTNULL(new RPCRequestContext));
    context->routes.reset(routes_);
    context->request_id.reset(data_->release(0));

    scoped_ptr<MessageVector> data(data_);
    CHECK_EQ(3, data->size());
    zmq::message_t& request = (*data)[1];
    zmq::message_t& payload = (*data)[2];

    GenericRPCRequest generic_rpc_request;
    VLOG(2) << "Received request of size " << request.size();
    if (!generic_rpc_request.ParseFromArray(request.data(), request.size())) {
      // Handle bad RPC.
      VLOG(2) << "Received corrupt message.";
      ReplyWithAppError(reply, *context,
                        GenericRPCResponse::INVALID_GENERIC_WRAPPER);
      return;
    };
    ServiceMap::const_iterator service_it = service_map_.find(
        generic_rpc_request.service());
    if (service_it == service_map_.end()) {
      // Handle invalid service.
      VLOG(2) << "Invalid service: " << generic_rpc_request.service();
      ReplyWithAppError(
          reply, *context, GenericRPCResponse::UNKNOWN_SERVICE);
      return;
    }
    zrpc::Service* service = service_it->second;
    const ::google::protobuf::MethodDescriptor* descriptor =
        service->GetDescriptor()->FindMethodByName(
            generic_rpc_request.method());
    if (descriptor == NULL) {
      // Invalid method name
      VLOG(2) << "Invalid method name: " << generic_rpc_request.method();
      ReplyWithAppError(reply, *context, GenericRPCResponse::UNKNOWN_METHOD);
      return;
    }
    context->request.reset(CHECK_NOTNULL(
        service->GetRequestPrototype(descriptor).New()));
    context->response.reset(CHECK_NOTNULL(
        service->GetResponsePrototype(descriptor).New()));
    if (!context->request->ParseFromArray(payload.data(), payload.size())) {
      VLOG(2) << "Failed to parse payload.";
      // Invalid proto;
      ReplyWithAppError(reply, *context,
                        GenericRPCResponse::INVALID_MESSAGE);
      return;
    }

    Closure *closure = NewCallback(
        &FinalizeResponse, context.get(), reply);
    context->rpc.SetStatus(GenericRPCResponse::OK);
    service->CallMethod(descriptor, &context->rpc,
                        context->request.get(),
                        context->response.get(), closure);
    context.release();
  }

  void HandleRequest(zmq::socket_t* fs_socket) {
    scoped_ptr<MessageVector> routes(new MessageVector());
    scoped_ptr<MessageVector> data(new MessageVector());
    ReadMessageToVector(socket_, routes.get(), data.get());
    if (data->size() != 3) {
      VLOG(2) << "Dropping invalid requests.";
      return;
    }
    FunctionServer::AddFunction(
        fs_socket,
        boost::bind(&ServerImpl::HandleRequestWorker,
                    this, routes.release(), data.release(), _1));
  }

  zmq::socket_t* socket_;
  FunctionServer* function_server_;
  typedef std::map<std::string, zrpc::Service*> ServiceMap;
  ServiceMap service_map_;
  DISALLOW_COPY_AND_ASSIGN(ServerImpl);
};

Server::Server(zmq::socket_t* socket, EventManager* event_manager)
    : server_impl_(new ServerImpl(socket, event_manager->GetFunctionServer())) {
}

void Server::Start() {
  server_impl_->Start();
}

void Server::RegisterService(zrpc::Service *service) {
  server_impl_->RegisterService(service);
}

}  // namespace
