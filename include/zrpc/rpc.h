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

#ifndef ZRPC_RPC_H
#define ZRPC_RPC_H

#include <string>
#include <zrpc/macros.h>
#include "zrpc/zrpc.pb.h"

namespace zrpc {
class Connection;
class SimpleRpcChannel;
struct RemoteResponse;

class RPC {
 public:
  RPC();

  ~RPC();

  inline bool OK() const {
    return GetStatus() == GenericRPCResponse::OK;
  }

  GenericRPCResponse::Status GetStatus() const {
    return status_;
  }

  inline std::string GetErrorMessage() const {
    return error_message_;
  }

  inline int GetApplicationError() const {
    return application_error_;
  }

  inline int64 GetDeadlineMs() const {
    return deadline_ms_;
  }

  inline void SetDeadlineMs(int deadline_ms) {
    deadline_ms_ = deadline_ms;
  }

  void SetFailed(const std::string& message);

  void SetFailed(int application_error, const std::string& message);

  int Wait();

 private:
  void SetStatus(GenericRPCResponse::Status status);

  GenericRPCResponse::Status status_;
  Connection* connection_;
  RemoteResponse* remote_response_;
  std::string error_message_;
  int application_error_;
  int64 deadline_ms_;

  friend class SimpleRpcChannel;
  friend class Server;
};
}  // namespace
#endif
