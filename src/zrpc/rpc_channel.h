// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_RPC_CHANNEL_H
#define ZRPC_RPC_CHANNEL_H

#include <string>
#include "macros.h"
#include "zrpc/event_manager_controller.h"

namespace google {
namespace protobuf {
class Message;
class MethodDescriptor;
}  // namespace protobuf
}  // namespace google

namespace zrpc {
struct ClientRequest;
class Connection;
class EventManager;
class EventManagerController;
class RPC;
struct  RPCResponseContext;

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

  virtual ~ZMQRpcChannel() {};

 private:
  virtual void HandleClientResponse(RPCResponseContext *response_context);

  scoped_ptr<EventManagerController> controller_;
  Connection* connection_;
};
}  // namespace
#endif
