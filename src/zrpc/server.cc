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

#include <boost/bind.hpp>
#include <glog/logging.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/stubs/common.h>
#include <zmq.hpp>

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
void SendGenericResponse(MessageVector* routes,
                         zmq::message_t* request_id,
                         const GenericRPCResponse& generic_rpc_response,
                         const StringPiece& payload,
                         FunctionServer::ReplyFunction reply) {
  std::string serialized_generic_response;
  size_t msg_size = generic_rpc_response.ByteSize();
  zmq::message_t* zmq_response_message = new zmq::message_t(msg_size);
  CHECK(generic_rpc_response.SerializeToArray(
      zmq_response_message->data(),
      msg_size));

  zmq::message_t* zmq_payload_message = new zmq::message_t(payload.size());
  memcpy(zmq_payload_message->data(), payload.data(), payload.size());

  routes->push_back(request_id);
  routes->push_back(zmq_response_message);
  routes->push_back(zmq_payload_message);
  reply(routes);
  delete routes;
}

void ReplyWithAppError(FunctionServer::ReplyFunction reply,
                       MessageVector* routes,
                       zmq::message_t* request_id,
                       int application_error,
                       const std::string& error="") {
  GenericRPCResponse response;
  response.set_status(GenericRPCResponse::APPLICATION_ERROR);
  response.set_application_error(application_error);
  if (!error.empty()) {
    response.set_error(error);
  }
  SendGenericResponse(routes,
                      request_id, 
                      response, StringPiece(), reply);
}

struct RPCRequestContext {
  RPC rpc;
  ::google::protobuf::Message* request;
  ::google::protobuf::Message* response;
  MessageVector* routes;
  zmq::message_t* request_id;
};

void FinalizeResponse(
    RPCRequestContext *context,
    FunctionServer::ReplyFunction reply) {
  GenericRPCResponse generic_rpc_response;
  std::string payload;
  if (context->rpc.OK()) {
    CHECK(context->response->SerializeToString(
          &payload));
  } else {
    generic_rpc_response.set_status(
        context->rpc.GetStatus());
    generic_rpc_response.set_application_error(
        context->rpc.GetApplicationError());
    std::string error_message(context->rpc.GetErrorMessage());
    if (!error_message.empty()) {
      generic_rpc_response.set_error(error_message);
    }
  }
  SendGenericResponse(context->routes,
                      context->request_id,
                      generic_rpc_response,
                      StringPiece(payload), reply); 
  delete context->request;
  delete context->response;
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
    data.erase(0);
    WriteVectorToSocket(socket_, data);
  }

  void HandleRequestWorker(MessageVector* routes_,
                           MessageVector* data_,
                           FunctionServer::ReplyFunction reply) {
    scoped_ptr<MessageVector> routes(routes_);
    scoped_ptr<MessageVector> data(data_);
    CHECK_EQ(3, data->size());
    zmq::message_t* request_id = new zmq::message_t;
    request_id->move((*data)[0]);
    zmq::message_t* const& request = (*data)[1];
    zmq::message_t* const& payload = (*data)[2];

    GenericRPCRequest generic_rpc_request;
    VLOG(2) << "Received request of size " << request->size();
    if (!generic_rpc_request.ParseFromArray(request->data(), request->size())) {
      // Handle bad RPC.
      VLOG(2) << "Received corrupt message.";
      ReplyWithAppError(reply, routes_, request_id,
                        GenericRPCResponse::INVALID_GENERIC_WRAPPER);
      return;
    };
    ServiceMap::const_iterator service_it = service_map_.find(
        generic_rpc_request.service());
    if (service_it == service_map_.end()) {
      // Handle invalid service.
      ReplyWithAppError(
          reply, routes_, request_id, GenericRPCResponse::UNKNOWN_SERVICE);
      return;
    }
    zrpc::Service* service = service_it->second;
    const ::google::protobuf::MethodDescriptor* descriptor =
        service->GetDescriptor()->FindMethodByName(
            generic_rpc_request.method());
    if (descriptor == NULL) {
      // Invalid method name
      ReplyWithAppError(reply, routes_,
                        request_id, GenericRPCResponse::UNKNOWN_METHOD);
      return;
    }
    RPCRequestContext* context = CHECK_NOTNULL(new RPCRequestContext);
    context->routes = routes.release();
    context->request_id = request_id;
    context->request = CHECK_NOTNULL(
        service->GetRequestPrototype(descriptor).New());
    context->response = CHECK_NOTNULL(
        service->GetResponsePrototype(descriptor).New());
    if (!context->request->ParseFromArray(payload->data(), payload->size())) {
      // Invalid proto;
      ReplyWithAppError(reply, routes_,
                        request_id, GenericRPCResponse::INVALID_MESSAGE);
      delete context->request;
      delete context->response;
      return;
    }
    Closure *closure = NewCallback(
        &FinalizeResponse, context, reply);
    context->rpc.SetStatus(GenericRPCResponse::OK);
    service->CallMethod(descriptor, &context->rpc,
                        context->request,
                        context->response, closure);
  }

  void HandleRequest(zmq::socket_t* fs_socket) {
    MessageVector* routes = new MessageVector();
    MessageVector* data = new MessageVector();
    ReadMessageToVector(socket_, routes, data);
    if (data->size() != 3) {
      VLOG(2) << "Dropping invalid requests.";
      delete routes;
      delete data;
      return;
    }
    FunctionServer::AddFunction(
        fs_socket,
        boost::bind(&ServerImpl::HandleRequestWorker,
                    this, routes, data, _1));
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
