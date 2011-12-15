// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <glog/logging.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/stubs/common.h>
#include <signal.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <zmq.h>
#include <zmq.hpp>
#include <iostream>
#include <utility>

#include "zrpc/callback.h"
#include "zrpc/connection_manager.h"
#include "zrpc/macros.h"
#include "zrpc/rpc.h"
#include "zrpc/reactor.h"
#include "zrpc/server.h"
#include "zrpc/service.h"
#include "zrpc/string_piece.h"
#include "zrpc/zmq_utils.h"
#include "zrpc/zrpc.pb.h"

namespace zrpc {

Server::Server(zmq::socket_t* socket) : socket_(socket) {}

void Server::Start() {
  InstallSignalHandler();
  Reactor reactor;
  reactor.AddSocket(socket_, NewPermanentCallback(
          this, &Server::HandleRequest));
  reactor.LoopUntil(NULL);
  LOG(INFO) << "Server shutdown.";
}

void Server::RegisterService(zrpc::Service *service) {
  VLOG(2) << "Registering service '" << service->GetDescriptor()->name() << "'";
  service_map_[service->GetDescriptor()->name()] = service;
}

namespace {
struct RPCRequestContext {
  RPC rpc;
  ::google::protobuf::Message* request;
  ::google::protobuf::Message* response;
  zmq::message_t* request_id;
};

void SendGenericResponse(::zmq::socket_t* socket,
                         zmq::message_t* request_id,
                         const GenericRPCResponse& generic_rpc_response,
                         const StringPiece& payload) {
  std::string serialized_generic_response;
  CHECK(generic_rpc_response.SerializeToString(&serialized_generic_response));
  zmq::message_t zmq_response_message(serialized_generic_response.length());
  memcpy(zmq_response_message.data(), serialized_generic_response.c_str(),
         serialized_generic_response.length());

  zmq::message_t zmq_payload_message(payload.size());
  memcpy(zmq_payload_message.data(), payload.data(), payload.size());

  socket->send(*request_id, ZMQ_SNDMORE);
  socket->send(zmq_response_message, ZMQ_SNDMORE);
  socket->send(zmq_payload_message, 0);
}

void FinalizeResponse(RPCRequestContext *context,
                      ::zmq::socket_t* socket) {
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
  SendGenericResponse(socket, context->request_id, generic_rpc_response,
                      StringPiece(payload)); 
  delete context->request;
  delete context->response;
  delete context;
}

void ReplyWithAppError(zmq::socket_t* socket,
                       zmq::message_t* request_id,
                       int application_error,
                       const std::string& error="") {
  GenericRPCResponse response;
  response.set_status(GenericRPCResponse::APPLICATION_ERROR);
  response.set_application_error(application_error);
  if (!error.empty()) {
    response.set_error(error);
  }
  SendGenericResponse(socket, request_id, response, StringPiece());
}
}

void Server::HandleRequest() {
  MessageVector data;
  ReadMessageToVector(socket_, &data);
  CHECK(data.size() == 3);
  zmq::message_t* const& request_id = data[0];
  zmq::message_t* const& request = data[1];
  zmq::message_t* const& payload = data[2];

  GenericRPCRequest generic_rpc_request;
  VLOG(2) << "Received request of size " << request->size();
  if (!generic_rpc_request.ParseFromArray(request->data(), request->size())) {
    // Handle bad RPC.
    VLOG(2) << "Received corrupt message.";
    ReplyWithAppError(socket_, request_id, GenericRPCResponse::INVALID_GENERIC_WRAPPER);
    return;
  };
  ServiceMap::const_iterator service_it = service_map_.find(
      generic_rpc_request.service());
  if (service_it == service_map_.end()) {
    // Handle invalid service.
    ReplyWithAppError(socket_, request_id, GenericRPCResponse::UNKNOWN_SERVICE);
    return;
  }
  zrpc::Service* service = service_it->second;
  const ::google::protobuf::MethodDescriptor* descriptor =
      service->GetDescriptor()->FindMethodByName(generic_rpc_request.method());
  if (descriptor == NULL) {
    // Invalid method name
    ReplyWithAppError(socket_, request_id, GenericRPCResponse::UNKNOWN_METHOD);
    return;
  }
  RPCRequestContext* context = CHECK_NOTNULL(new RPCRequestContext);
  context->request_id = request_id;
  context->request = CHECK_NOTNULL(
      service->GetRequestPrototype(descriptor).New());
  context->response = CHECK_NOTNULL(
      service->GetResponsePrototype(descriptor).New());
  if (!context->request->ParseFromArray(payload->data(), payload->size())) {
    // Invalid proto;
    ReplyWithAppError(socket_, request_id, GenericRPCResponse::INVALID_MESSAGE);
    delete context->request;
    delete context->response;
    return;
  }

  Closure *closure = NewCallback(
      &FinalizeResponse, context, socket_);
  context->rpc.SetStatus(GenericRPCResponse::OK);
  service->CallMethod(descriptor, &context->rpc,
                      context->request,
                      context->response, closure);
}
}  // namespace
