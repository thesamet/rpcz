// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <glog/logging.h>
#include "google/protobuf/descriptor.h"
#include "rpc_channel.h"
#include "zmq.hpp"
#include "zrpc/event_manager.h"
#include "zrpc/rpc.h"
#include "zrpc/zrpc.pb.h"

namespace zrpc {

ZMQRpcChannel::ZMQRpcChannel(
    EventManagerController* controller,
    Connection* connection) :
    controller_(controller),
    connection_(connection) {};

struct RPCResponseContext {
  RPC* rpc;
  ClientRequest client_request;
  ::google::protobuf::Message* response;
  Closure* user_closure;
};

void ZMQRpcChannel::CallMethod(
    const google::protobuf::MethodDescriptor* method,
    RPC* rpc,
    const google::protobuf::Message* request,
    google::protobuf::Message* response,
    google::protobuf::Closure* done) {
  GenericRPCRequest generic_request;
  generic_request.set_service(method->service()->name());
  generic_request.set_method(method->name());
  generic_request.set_payload(request->SerializeAsString());

  std::string msg_request = generic_request.SerializeAsString();
  zmq::message_t msg_out(msg_request.size());
  memcpy(msg_out.data(), msg_request.c_str(), msg_request.size());
  std::vector<zmq::message_t*> vout(1, &msg_out);
  RPCResponseContext *response_context = new RPCResponseContext;
  response_context->rpc = rpc;
  response_context->user_closure = done;
  response_context->response = response;
  response_context->client_request.closure = NewCallback(
      this,
      &ZMQRpcChannel::HandleClientResponse,
      response_context);
  controller_->Forward(connection_,
                       &response_context->client_request,
                       vout);
}

void ZMQRpcChannel::HandleClientResponse(
    RPCResponseContext* response_context) {
  GenericRPCResponse generic_response;
  zmq::message_t& msg_in = *response_context->client_request.result[0];
  CHECK(generic_response.ParseFromArray(msg_in.data(), msg_in.size()));
  if (generic_response.status() != GenericRPCResponse::OK) {
    LOG(INFO) << generic_response.application_error();
    response_context->rpc->SetFailed(generic_response.application_error(),
                                     generic_response.error());
  } else {
    CHECK(response_context->response->ParseFromString(
            generic_response.payload())); 
  }
  response_context->user_closure->Run();
  delete response_context;
}
}  // namespace zrpc
