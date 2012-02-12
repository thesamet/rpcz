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
class client_connection;
class connection_manager;
class message_iterator;
class rpc_service;
class server_channel;
class service;

// server receives incoming rpc requests on a socket. When such a request
// arrives it forwards it to an event manager for processing, and later passes
// the response back to the caller. This class is not thread-safe: construct it
// and use it from a single thread.
class server {
 public:
  // Creates a server that will receive requests from the given socket that has
  // already been bound to some endpoint. We assume that the socket is of ROUTER
  // type. The provided event manager will be used to handle the requests.
  // The server takes ownership of the socket, but not of event_manager.
  server(connection_manager* connection_manager);

  ~server();

  // Registers an rpc service with this server. All registrations must occur
  // before bind() is called. The name parameter identifies the service for
  // external clients. If you use the first form, the service name from the
  // protocol buffer definition will be used. Does not take ownership of the
  // provided service.
  void register_service(service* service);
  void register_service(service* service, const std::string& name);

  void bind(const std::string& endpoint);

  // Registers a low-level rpc_service.
  void register_service(rpc_service* rpc_service, const std::string& name);

 private:
  void handle_request(const client_connection& connection,
                     message_iterator& iter);

  connection_manager* connection_manager_;
  typedef std::map<std::string, rpcz::rpc_service*> rpc_service_map;
  rpc_service_map service_map_;
  DISALLOW_COPY_AND_ASSIGN(server);
};

// rpc_service is a low-level request handler: requests and replies are void*.
// It is exposed here for language bindings. Do not use directly.
class rpc_service {
 public:
  virtual ~rpc_service() {}

  virtual void dispatch_request(const std::string& method,
                               const void* payload, size_t payload_len,
                               server_channel* channel_) = 0;
};
}  // namespace
#endif
