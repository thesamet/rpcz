// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_RPC_H
#define ZRPC_RPC_H

#include <string>
#include "zrpc/proto/zrpc.pb.h"

namespace zrpc {

class RPC {
 public:
  RPC();

  inline bool OK() {
    return GetStatus() == GenericRPCResponse::OK;
  }

  GenericRPCResponse::Status GetStatus() {
    return status_;
  }

  inline std::string GetErrorMessage() {
    return error_message_;
  }

  inline int GetApplicationError() {
    return application_error_;
  }

  void SetFailed(const std::string& message);

  void SetFailed(int application_error, const std::string& message);

 private:
  GenericRPCResponse::Status status_;
  std::string error_message_;
  int application_error_;
};

}  // namespace
#endif
