// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_RPC_H
#define ZRPC_RPC_H

#include <string>
#include "zrpc/zrpc.pb.h"

namespace zrpc {
class ZMQRpcChannel;
struct RpcResponseContext;

class RPC {
 public:
  RPC();

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

  void SetFailed(const std::string& message);

  void SetFailed(int application_error, const std::string& message);

  GenericRPCResponse::Status Wait();

 private:
  void SetStatus(GenericRPCResponse::Status status);

  GenericRPCResponse::Status status_;
  ZMQRpcChannel* rpc_channel_;
  RpcResponseContext* rpc_response_context_;
  std::string error_message_;
  int application_error_;

  friend class ZMQRpcChannel;
  friend class Server;
};
}  // namespace
#endif
