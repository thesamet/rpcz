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
#include "zrpc/clock.h"
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
  ::google::protobuf::Message* response_msg;
  std::string* response_str;
  Closure* user_closure;
  ClientRequest client_request;
};

void ZMQRpcChannel::CallMethodFull(
    const std::string& service_name,
    const std::string& method_name,
    RPC* rpc,
    const std::string& request,
    std::string* response_str,
    ::google::protobuf::Message* response_msg,
    google::protobuf::Closure* done) {
  CHECK(rpc->GetStatus() == GenericRPCResponse::INACTIVE);
  GenericRPCRequest generic_request;
  generic_request.set_service(service_name);
  generic_request.set_method(method_name);

  std::string msg_request = generic_request.SerializeAsString();
  zmq::message_t* msg_out = new zmq::message_t(msg_request.size());
  memcpy(msg_out->data(), msg_request.c_str(), msg_request.size());

  zmq::message_t* payload_out = new zmq::message_t(request.size());
  memcpy(payload_out->data(), request.c_str(), request.size());

  MessageVector vout;
  vout.push_back(msg_out);
  vout.push_back(payload_out);

  RpcResponseContext *response_context = new RpcResponseContext;
  response_context->client_request.closure = NewCallback(
      this, &ZMQRpcChannel::HandleClientResponse,
      response_context);
  response_context->rpc = rpc;
  response_context->client_request.deadline_ms = rpc->GetDeadlineMs();
  response_context->client_request.start_time = zclock_time();
  response_context->user_closure = done;
  response_context->response_str = response_str;
  response_context->response_msg = response_msg;
  rpc->SetStatus(GenericRPCResponse::INFLIGHT);
  rpc->rpc_channel_ = this;
  rpc->rpc_response_context_ = response_context;

  controller_->Forward(connection_,
                       &response_context->client_request,
                       vout);
}

void ZMQRpcChannel::CallMethod0(const std::string& service_name,
                                const std::string& method_name,
                                RPC* rpc,
                                const std::string& request,
                                std::string* response,
                                google::protobuf::Closure* done) {
  CallMethodFull(service_name,
                 method_name,
                 rpc,
                 request,
                 response,
                 NULL,
                 done);
}

void ZMQRpcChannel::CallMethod(
    const google::protobuf::MethodDescriptor* method,
    RPC* rpc,
    const google::protobuf::Message* request,
    google::protobuf::Message* response,
    google::protobuf::Closure* done) {
  CallMethodFull(method->service()->name(),
                 method->name(),
                 rpc,
                 request->SerializeAsString(),
                 NULL,
                 response,
                 done);
}

void ZMQRpcChannel::HandleClientResponse(
    RpcResponseContext* response_context) {
  ClientRequest& client_request = response_context->client_request;

  switch (client_request.status) {
    case ClientRequest::DEADLINE_EXCEEDED:
      response_context->rpc->SetStatus(
          GenericRPCResponse::DEADLINE_EXCEEDED);
      break;
    case ClientRequest::DONE: {
        GenericRPCResponse generic_response;
        zmq::message_t& msg_in = *response_context->client_request.result[0];
        CHECK(generic_response.ParseFromArray(msg_in.data(), msg_in.size()));
        if (generic_response.status() != GenericRPCResponse::OK) {
          response_context->rpc->SetFailed(generic_response.application_error(),
                                           generic_response.error());
        } else {
          response_context->rpc->SetStatus(GenericRPCResponse::OK);
          if (response_context->response_msg) {
            CHECK(response_context->response_msg->ParseFromArray(
                    response_context->client_request.result[1]->data(),
                    response_context->client_request.result[1]->size()));
          } else if (response_context->response_str) {
            response_context->response_str->assign(
                static_cast<char*>(
                    response_context->client_request.result[1]->data()),
                response_context->client_request.result[1]->size());
          }
        }
      }
      break;
    case ClientRequest::ACTIVE:
    case ClientRequest::INACTIVE:
    default:
      CHECK(false) << "Unexpected ClientRequest state: "
          << client_request.status;
  }
  if (response_context->user_closure) {
    response_context->user_closure->Run();
  }
  waiting_on_.erase(response_context);
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

int ZMQRpcChannel::WaitFor(RpcResponseContext* response_context) {
  waiting_on_.insert(response_context);
  RequestStoppingCondition condition(&waiting_on_);
  return controller_->WaitFor(&condition);
}
}  // namespace zrpc
