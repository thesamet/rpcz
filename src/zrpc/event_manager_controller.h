// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_EVENT_MANAGER_CONTROLLER_H
#define ZRPC_EVENT_MANAGER_CONTROLLER_H

#include <string>
#include <vector>

namespace zmq {
class message_t;
}  // namespace zmq

namespace zrpc {

struct ClientRequest;
class Connection;

// Controls an event manager.
class EventManagerController {
 public:
  virtual void AddRemoteEndpoint(Connection* connection,
                                 const std::string& remote_endpoint) = 0;

  virtual void Forward(Connection* connection,
                       ClientRequest* client_request,
                       const std::vector<zmq::message_t*>& messages) = 0;

  virtual void Quit() = 0;

  virtual ~EventManagerController() {};
};
}  // namespace zrpc
#endif
