// Copyright 2011 Google Inc. All Rights Reserved.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: nadavs@google.com <Nadav Samet>

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
  virtual ~ConnectionManagerController() {};

  virtual void AddRemoteEndpoint(Connection* connection,
                                 const std::string& remote_endpoint) = 0;

  virtual void Forward(Connection* connection,
               const MessageVector& request,
               RemoteResponse* remote_response,
               int64 deadline_ms,
               Closure* closure) = 0;

  virtual int WaitUntil(StoppingCondition* client_request) = 0;

  virtual void Quit() = 0;
};
}  // namespace zrpc
#endif
