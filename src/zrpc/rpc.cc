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

#include "glog/logging.h"
#include "zrpc/connection_manager.h"
#include "zrpc/reactor.h"
#include "zrpc/rpc.h"
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
