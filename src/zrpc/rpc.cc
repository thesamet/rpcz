// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "zrpc/zrpc.pb.h"
#include "zrpc/rpc.h"

namespace zrpc {

RPC::RPC() : status_(GenericRPCResponse::OK), application_error_(0) {
};

void RPC::SetFailed(const std::string& error_message) {
  SetFailed(GenericRPCResponse::UNKNOWN_APPLICATION_ERROR, error_message);
}

void RPC::SetFailed(int application_error, const std::string& error_message) {
  error_message_ = error_message;
  status_ = GenericRPCResponse::APPLICATION_ERROR;
  application_error_ = application_error;
}
}  // namespace zrpc
