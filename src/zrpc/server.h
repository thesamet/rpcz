// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_SERVER_H
#define ZRPC_SERVER_H

#include <map>
#include <string>

namespace zmq {
class socket_t;
class message_t;
};

namespace zrpc {
class Service;
}

namespace zrpc {

class Server {
 public:
  Server(zmq::socket_t* socket);

  void Start();

  void RegisterService(zrpc::Service *service);

 private:
  void HandleRequest(zmq::message_t* request);

  zmq::socket_t* socket_;
  typedef std::map<std::string, zrpc::Service*> ServiceMap;
  ServiceMap service_map_;
};

}  // namespace
#endif
