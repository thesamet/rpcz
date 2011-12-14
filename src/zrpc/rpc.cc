// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "glog/logging.h"
#include "zrpc/event_manager.h"
#include "zrpc/reactor.h"
#include "zrpc/rpc.h"
#include "zrpc/thread_rpc_channel.h"
#include "zrpc/zrpc.pb.h"

namespace zrpc {

RPC::RPC()
    : status_(GenericRPCResponse::INACTIVE),
      connection_(NULL),
      rpc_response_context_(NULL),
      application_error_(0),
      deadline_ms_(-1) {
};

RPC::~RPC() {}

void RPC::SetFailed(const std::string& error_message) {
  SetFailed(GenericRPCResponse::UNKNOWN_APPLICATION_ERROR, error_message);
}

void RPC::SetStatus(GenericRPCResponse::Status status) {
  status_ = status;
}

void RPC::SetFailed(int application_error, const std::string& error_message) {
  SetStatus(GenericRPCResponse::APPLICATION_ERROR);
  error_message_ = error_message;
  application_error_ = application_error;
}

class IsRpcDone : public StoppingCondition {
 public:
  IsRpcDone(RPC* rpc) : rpc_(rpc) {};

  bool ShouldStop() {
    return (rpc_->GetStatus() != GenericRPCResponse::INFLIGHT);
  }

 private:
  RPC* rpc_;
};

int RPC::Wait() {
  GenericRPCResponse::Status status = GetStatus();
  CHECK_NE(status, GenericRPCResponse::INACTIVE)
      << "Request must be sent before calling Wait()";
  if (status != GenericRPCResponse::INFLIGHT) {
    return GetStatus();
  }
  IsRpcDone is_rpc_done(this);
  return connection_->WaitUntil(&is_rpc_done);
}
}  // namespace zrpc
