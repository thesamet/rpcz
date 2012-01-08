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

#ifndef RPCZ_SERVER_H
#define RPCZ_SERVER_H

#include "rpcz/macros.h"
#include "rpcz/rpcz.pb.h"

namespace zmq {
class socket_t;
};

namespace rpcz {
class EventManager;
class RpcService;
class ServerChannel;
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
  // The Server takes ownership of the socket, but not of event_manager.
  Server(zmq::socket_t* socket, EventManager* event_manager);

  ~Server();

  // Registers an RPC Service with this server. All registrations must occur
  // before Start() is called. The name parameter identifies the service for
  // external clients. If you use the first form, the service name from the
  // protocol buffer definition will be used. Does not take ownership of the
  // provided service.
  void RegisterService(Service* service);
  void RegisterService(Service* service, const std::string& name);

  // Registers a low-level RpcService.
  void RegisterService(RpcService* rpc_service, const std::string& name);

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

// RpcService is a low-level request handler: requests and replies are void*.
// It is exposed here for language bindings. Do not use directly.
class RpcService {
 public:
  virtual void DispatchRequest(const std::string& method,
                               const void* payload, size_t payload_len,
                               ServerChannel* channel_) = 0;
};
}  // namespace
#endif
