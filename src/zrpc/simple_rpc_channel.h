#ifndef ZRPC_SIMPLE_RPC_CHANNEL_H_
#define ZRPC_SIMPLE_RPC_CHANNEL_H_

#include "zrpc/rpc_channel.h"

namespace zrpc {

class Connection;
class ClientRequest;
class Closure;
class ConnectionManager;
struct RpcResponseContext;

class SimpleRpcChannel: public RpcChannel {
 public:
  SimpleRpcChannel(Connection* connection);

  virtual ~SimpleRpcChannel();

  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          RPC* rpc, const google::protobuf::Message* request,
                          google::protobuf::Message* response, Closure* done);

  virtual void CallMethod0(
      const std::string& service_name,
      const std::string& method_name, RPC* rpc,
      const std::string& request,
      std::string* response, Closure* done);

 private:
  virtual void HandleClientResponse(RpcResponseContext *response_context);

  void CallMethodFull(
    const std::string& service_name,
    const std::string& method_name,
    RPC* rpc,
    const std::string& request,
    std::string* response_str,
    ::google::protobuf::Message* response_msg,
    Closure* done);

  Connection* connection_;
};
} // namespace zrpc
#endif /* ZRPC_SIMPLE_RPC_CHANNEL_H_ */
