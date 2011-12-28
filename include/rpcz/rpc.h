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

#ifndef RPCZ_RPC_H
#define RPCZ_RPC_H

#include <stdexcept>
#include <string>

#include "rpcz/macros.h"
#include "rpcz/rpcz.pb.h"

namespace rpcz {
class SyncEvent;

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

  void SetFailed(int application_error, const std::string& message);

  int Wait();

  std::string ToString() const;

 private:
  void SetStatus(GenericRPCResponse::Status status);

  GenericRPCResponse::Status status_;
  std::string error_message_;
  int application_error_;
  int64 deadline_ms_;
  scoped_ptr<SyncEvent> sync_event_;

  friend class RpcChannelImpl;
  friend class ServerImpl;
};

class RpcError : public std::runtime_error {
 public:
  explicit RpcError(const RPC& rpc_) : std::runtime_error(rpc_.ToString()) {}
};

class InvalidMessageError : public std::runtime_error {
 public:
  explicit InvalidMessageError(const std::string& message)
      : std::runtime_error(message) {}
};
}  // namespace
#endif
