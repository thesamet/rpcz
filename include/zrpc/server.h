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

#ifndef ZRPC_SERVER_H
#define ZRPC_SERVER_H

#include "zrpc/macros.h"

namespace zmq {
class socket_t;
};

namespace zrpc {
class EventManager;
class ServerImpl;
class Service;

class Server {
 public:
  Server(zmq::socket_t* socket, EventManager* event_manager);

  ~Server();

  void Start();

  void RegisterService(Service *service);

 private:
  scoped_ptr<ServerImpl> server_impl_;
  DISALLOW_COPY_AND_ASSIGN(Server);
};
}  // namespace
#endif
