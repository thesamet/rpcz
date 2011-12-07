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

#include "zrpc/rpc.h"
#include "zrpc/server.h"
#include "zrpc/service.h"
#include "zrpc/zrpc.pb.h"

namespace zrpc {

namespace {
static int s_interrupted = 0;
static void s_signal_handler (int signal_value)
{
  LOG(INFO) << "Received " << ((signal_value == SIGTERM) ? "SIGTERM" :
                               (signal_value == SIGINT) ? "SIGINT" : "signal")
                           << ".";
  s_interrupted = 1;
}

static void s_catch_signals (void)
{
    struct sigaction action;
    action.sa_handler = s_signal_handler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGTERM, &action, NULL);
}
}

Server::Server(zmq::socket_t* socket) : socket_(socket) {}

void Server::Start() {
  s_catch_signals();
  int requests = 0;
  while (!s_interrupted && requests < 1000) {
    ++requests;
    zmq::message_t request_id;
    zmq::message_t request;
    try {
      socket_->recv(&request_id);
    } catch (zmq::error_t &e) {
      if (e.num() == EINTR) {
        VLOG(2) << "recv() interrupted.";
        // Interrupts are ok, if we should stop, s_interrupted will be set for
        // us.
        continue;
      }
      throw;
    }
    socket_->recv(&request);
    HandleRequest(&request_id, &request);
  }
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
                         const GenericRPCResponse& generic_rpc_response) {
  std::string serialized_generic_response;
  CHECK(generic_rpc_response.SerializeToString(&serialized_generic_response));
  zmq::message_t zmq_response_message(serialized_generic_response.length());
  memcpy(zmq_response_message.data(), serialized_generic_response.c_str(),
         serialized_generic_response.length());
  socket->send(*request_id, ZMQ_SNDMORE);
  socket->send(zmq_response_message);
}

void FinalizeResponse(RPCRequestContext *context,
                      ::zmq::socket_t* socket) {
  GenericRPCResponse generic_rpc_response;
  if (context->rpc.OK()) {
    CHECK(context->response->SerializeToString(
            generic_rpc_response.mutable_payload()));
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
  SendGenericResponse(socket, context->request_id, generic_rpc_response); 
  delete context->request;
  delete context->response;
  delete context;
}

void ReplyWithAppError(zmq::socket_t* socket,
                       zmq::message_t *request_id,
                       int application_error,
                       const std::string& error="") {
  GenericRPCResponse response;
  response.set_status(GenericRPCResponse::APPLICATION_ERROR);
  response.set_application_error(application_error);
  if (!error.empty()) {
    response.set_error(error);
  }
  SendGenericResponse(socket, request_id, response);
}
}

void Server::HandleRequest(zmq::message_t* request_id, zmq::message_t* request) {
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
  if (!context->request->ParseFromString(generic_rpc_request.payload())) {
    // Invalid proto;
    ReplyWithAppError(socket_, request_id, GenericRPCResponse::INVALID_MESSAGE);
    delete context->request;
    delete context->response;
    return;
  }

  ::google::protobuf::Closure *closure = ::google::protobuf::NewCallback(
      &FinalizeResponse, context, socket_);
  context->rpc.SetStatus(GenericRPCResponse::OK);
  service->CallMethod(descriptor, &context->rpc,
                      context->request,
                      context->response, closure);
}

}  // namespace
