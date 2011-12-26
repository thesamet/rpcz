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

// Server receives incoming RPC requests on a socket. When such a request
// arrives it forwards it to an event manager for processing, and later passes
// the response back to the caller. This class is not thread-safe: construct it
// and use it from a single thread.
class Server {
 public:
  // Creates a server that will receive requests from the given socket that has
  // already been bound to some endpoint. We assume that the socket is of ROUTER
  // type. The provided event manager will be used to handle the requests.
  // The Server does not take ownership of the socket, unless owns_socket is
  // true. It also does not take ownership of event_manager.
  Server(zmq::socket_t* socket, EventManager* event_manager,
         bool owns_socket=false);

  ~Server();

  // Registers an RPC Service with this server. All registrations must occur
  // before Start() is called.
  void RegisterService(Service *service);

  // Starts serving requests. The calling thread starts forwarding requests
  // from the socket to the event manager for processings. Only one thread may
  // call this function. The calling thread gets blocked until this function
  // returns. 
  // Currently, the only way to get the control back to the executing thread is
  // call InstallSignalHandler() and send the process a SIGTERM or SIGINT
  // (Ctrl-C).
  void Start();

 private:
  scoped_ptr<ServerImpl> server_impl_;
  DISALLOW_COPY_AND_ASSIGN(Server);
};
}  // namespace
#endif
