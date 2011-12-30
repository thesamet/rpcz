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

typedef RpcResponseHeader::Status Status;
typedef RpcResponseHeader::ApplicationError ApplicationError;

namespace status {
static const Status INACTIVE = RpcResponseHeader::INACTIVE;
static const Status ACTIVE = RpcResponseHeader::ACTIVE;
static const Status OK = RpcResponseHeader::OK;
static const Status CANCELLED = RpcResponseHeader::CANCELLED;
static const Status APPLICATION_ERROR = RpcResponseHeader::APPLICATION_ERROR;
static const Status DEADLINE_EXCEEDED = RpcResponseHeader::DEADLINE_EXCEEDED;
static const Status TERMINATED = RpcResponseHeader::TERMINATED;
}  // namespace status
namespace application_error {
static const ApplicationError NO_ERROR =
    RpcResponseHeader::NO_ERROR;
static const ApplicationError INVALID_HEADER =
    RpcResponseHeader::INVALID_HEADER;
static const ApplicationError NO_SUCH_SERVICE =
    RpcResponseHeader::NO_SUCH_SERVICE;
static const ApplicationError NO_SUCH_METHOD =
    RpcResponseHeader::NO_SUCH_METHOD;
static const ApplicationError INVALID_MESSAGE =
    RpcResponseHeader::INVALID_MESSAGE;
static const ApplicationError METHOD_NOT_IMPLEMENTED =
    RpcResponseHeader::METHOD_NOT_IMPLEMENTED;
}  // namespace application_error

class SyncEvent;

class RPC {
 public:
  RPC();

  ~RPC();

  inline bool OK() const {
    return GetStatus() == status::OK;
  }

  Status GetStatus() const {
    return status_;
  }

  inline std::string GetErrorMessage() const {
    return error_message_;
  }

  inline int GetApplicationErrorCode() const {
    return application_error_code_;
  }

  inline int64 GetDeadlineMs() const {
    return deadline_ms_;
  }

  inline void SetDeadlineMs(int deadline_ms) {
    deadline_ms_ = deadline_ms;
  }

  void SetFailed(int application_error_code, const std::string& message);

  int Wait();

  std::string ToString() const;

 private:
  void SetStatus(Status status);

  Status status_;
  std::string error_message_;
  int application_error_code_;
  int64 deadline_ms_;
  scoped_ptr<SyncEvent> sync_event_;

  friend class RpcChannelImpl;
  friend class ServerChannelImpl;
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
