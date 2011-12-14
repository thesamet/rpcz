// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_RPC_H
#define ZRPC_RPC_H

#include <string>
#include <zrpc/macros.h>
#include "zrpc/zrpc.pb.h"

namespace zrpc {
class Connection;
class SimpleRpcChannel;
struct RpcResponseContext;

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
  RpcResponseContext* rpc_response_context_;
  std::string error_message_;
  int application_error_;
  int64 deadline_ms_;

  friend class SimpleRpcChannel;
  friend class Server;
};
}  // namespace
#endif
