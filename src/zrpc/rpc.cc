// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "glog/logging.h"
#include "zrpc/zrpc.pb.h"
#include "zrpc/rpc.h"
#include "zrpc/rpc_channel.h"

namespace zrpc {

RPC::RPC()
    : status_(GenericRPCResponse::INACTIVE),
      rpc_channel_(NULL),
      rpc_response_context_(NULL),
      application_error_(0) {
};

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

GenericRPCResponse::Status RPC::Wait() {
  GenericRPCResponse::Status status = GetStatus();
  CHECK_NE(status, GenericRPCResponse::INACTIVE)
      << "Request must be sent before calling Wait()";
  if (status != GenericRPCResponse::INFLIGHT) {
    return GetStatus();
  }
  rpc_channel_->WaitFor(this->rpc_response_context_);
  return GetStatus();
}
}  // namespace zrpc
