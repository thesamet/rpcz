// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <google/protobuf/message.h>
#include <string.h>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "google/protobuf/descriptor.h"
#include "zmq.hpp"
#include "zrpc/client_request.h"
#include "zrpc/event_manager.h"
#include "zrpc/event_manager_controller.h"
#include "zrpc/macros.h"
#include "zrpc/reactor.h"
#include "zrpc/rpc.h"
#include "zrpc/rpc_channel.h"
#include "zrpc/zmq_rpc_channel.h"
#include "zrpc/zrpc.pb.h"

namespace zrpc {

ZMQRpcChannel::ZMQRpcChannel(
    EventManagerController* controller,
    Connection* connection)
    : controller_(controller),
      connection_(connection) {};

ZMQRpcChannel::~ZMQRpcChannel() {};

struct RpcResponseContext {
  RPC* rpc;
  ::google::protobuf::Message* response;
  Closure* user_closure;
  ClientRequest client_request;
};

void ZMQRpcChannel::CallMethod(
    const google::protobuf::MethodDescriptor* method,
    RPC* rpc,
    const google::protobuf::Message* request,
    google::protobuf::Message* response,
    google::protobuf::Closure* done) {
  CHECK(rpc->GetStatus() == GenericRPCResponse::INACTIVE);
  GenericRPCRequest generic_request;
  generic_request.set_service(method->service()->name());
  generic_request.set_method(method->name());
  generic_request.set_payload(request->SerializeAsString());

  std::string msg_request = generic_request.SerializeAsString();
  zmq::message_t msg_out(msg_request.size());
  memcpy(msg_out.data(), msg_request.c_str(), msg_request.size());
  std::vector<zmq::message_t*> vout(1, &msg_out);
  RpcResponseContext *response_context = new RpcResponseContext;
  response_context->client_request.closure = NewCallback(
      this, &ZMQRpcChannel::HandleClientResponse,
      response_context);
  response_context->rpc = rpc;
  response_context->user_closure = done;
  response_context->response = response;
  rpc->status_ = GenericRPCResponse::INFLIGHT;
  rpc->rpc_channel_ = this;
  rpc->rpc_response_context_ = response_context;

  controller_->Forward(connection_,
                       &response_context->client_request,
                       vout);
}

void ZMQRpcChannel::HandleClientResponse(
    RpcResponseContext* response_context) {
  GenericRPCResponse generic_response;
  zmq::message_t& msg_in = *response_context->client_request.result[0];
  CHECK(generic_response.ParseFromArray(msg_in.data(), msg_in.size()));
  if (generic_response.status() != GenericRPCResponse::OK) {
    LOG(INFO) << generic_response.application_error();
    LOG(INFO) << generic_response.error();
    response_context->rpc->SetFailed(generic_response.application_error(),
                                     generic_response.error());
  } else {
    CHECK(response_context->response->ParseFromString(
            generic_response.payload())); 
  }
  response_context->user_closure->Run();
  waiting_on_.erase(response_context);
  DeleteContainerPointers(response_context->client_request.result.begin(),
                          response_context->client_request.result.end());
  DeleteContainerPointers(response_context->client_request.return_path.begin(),
                          response_context->client_request.return_path.end());
  delete response_context;
}

class RequestStoppingCondition : public StoppingCondition {
 public:
  RequestStoppingCondition(std::set<RpcResponseContext*>* wait_set) :
      wait_set_(wait_set) {}

  bool ShouldStop() {
    return wait_set_->empty();
  }

 private:
  std::set<RpcResponseContext*>* wait_set_;
};

void ZMQRpcChannel::WaitFor(RpcResponseContext* response_context) {
  waiting_on_.insert(response_context);
  RequestStoppingCondition condition(&waiting_on_);
  controller_->WaitFor(&condition);
}
}  // namespace zrpc
