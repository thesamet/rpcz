// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_SERVER_H
#define ZRPC_SERVER_H

#include <map>
#include <string>

namespace zmq {
class zmq::socket_t;
};

namespace google {
namespace protobuf {
class Service;
}
}

namespace zrpc {

class Server {
 public:
  Server(zmq::socket_t* socket);

  void Start();

  void RegisterService(::google::protobuf::Service *service);

 private:
  void HandleRequest(zmq::message_t* request);

  zmq::socket_t* socket_;
  typedef std::map<std::string, ::google::protobuf::Service*> ServiceMap;
  ServiceMap service_map_;
};

}  // namespace
#endif
