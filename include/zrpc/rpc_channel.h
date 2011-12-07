// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_RPC_CHANNEL_H
#define ZRPC_RPC_CHANNEL_H

#include <string>
#include <set>

#include "google/protobuf/stubs/common.h"
#include "zrpc/macros.h"

namespace google {
namespace protobuf {
class Message;
class MethodDescriptor;
}  // namespace protobuf
}  // namespace google

namespace zrpc {
class Connection;
class EventManager;
class EventManagerController;
class RPC;
struct ClientRequest;
struct RpcResponseContext;

class RpcChannel {
 public:
  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          RPC* rpc,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          Closure* done) = 0;
  virtual ~RpcChannel() {};
};

class ZMQRpcChannel : public RpcChannel {
 public:
  ZMQRpcChannel(EventManagerController* controller, Connection* connection);

  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          RPC* rpc,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          google::protobuf::Closure* done);

  virtual void WaitFor(RpcResponseContext* response_context);

  virtual ~ZMQRpcChannel();

 private:
  virtual void HandleClientResponse(RpcResponseContext *response_context);

  scoped_ptr<EventManagerController> controller_;
  std::set<RpcResponseContext*> waiting_on_;  // set of requests we WaitFor.
  Connection* connection_;
  friend class RequestStoppingCondition;
};
}  // namespace
#endif
