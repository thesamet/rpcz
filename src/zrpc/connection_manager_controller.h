// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_CONNECTION_MANAGER_CONTROLLER_H
#define ZRPC_CONNECTION_MANAGER_CONTROLLER_H

#include <string>
#include <vector>
#include "zrpc/zmq_utils.h"

namespace zrpc {

struct ClientRequest;
class Connection;
class StoppingCondition;

// Controls an event manager.
class ConnectionManagerController {
 public:
  virtual void AddRemoteEndpoint(Connection* connection,
                                 const std::string& remote_endpoint) = 0;

  virtual void Forward(Connection* connection,
                       ClientRequest* client_request,
                       const MessageVector& messages) = 0;

  virtual int WaitUntil(StoppingCondition* client_request) = 0;

  virtual void Quit() = 0;

  virtual ~ConnectionManagerController() {};
};
}  // namespace zrpc
#endif
