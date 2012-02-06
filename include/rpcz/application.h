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

#ifndef RPCZ_APPLICATION_H
#define RPCZ_APPLICATION_H

#include <string>
#include "rpcz/macros.h"

namespace zmq {
class context_t; 
}  // namespace zmq

namespace rpcz {
class ConnectionManager;
class EventManager;
class RpcChannel;
class Server;

// rpcz::Application is a simple interface that helps setting up a common
// RPCZ client or server application.
class Application {
 public:
  class Options {
   public:
    Options() : connection_manager_threads(10),
                zeromq_context(NULL),
                zeromq_io_threads(1) {}

    // Number of connection manager threads. Each connection manager thread will
    // start a socket to any server we are talking to, so most applications
    // should keep this set to 1.
    int connection_manager_threads;

    // Number of event manager threads. For servers, those threads are going to
    // be used to process requests. For clients, those threads are going to be
    // used to execute callbacks when the response from a server is available.
    int event_manager_threads;

    // ZeroMQ context to use for our application. If NULL, then Application will
    // construct its own ZeroMQ context and own it. If you provide your own
    // ZeroMQ context, Application will not take ownership of it. The ZeroMQ
    // context must outlive the application.
    zmq::context_t* zeromq_context;

    // Number of ZeroMQ I/O threads, to be passed to zmq_init(). This value is
    // ignored when you provide your own ZeroMQ context.
    int zeromq_io_threads;
  };

  Application();

  explicit Application(const Options& options);

  virtual ~Application();

  // Creates an RpcChannel to the given endpoint. Attach it to a Stub and you
  // can start making calls through this channel from any thread. No locking
  // needed. It is your responsibility to delete this object.
  virtual RpcChannel* CreateRpcChannel(const std::string& endpoint);

  // Creates a server that listens to connection on the provided endpoint.
  // Call RegisterServer on the provided object to add services your
  // implemented, and then call Start(). The calling thread will start serving
  // requests (by forwarding them to the event manager).
  virtual Server* CreateServer(const std::string& endpoint);

 private:
  void Init(const Options& options);

  bool owns_context_;
  zmq::context_t* context_;
  scoped_ptr<ConnectionManager> connection_manager_;
  scoped_ptr<EventManager> event_manager_;
};
}  // namespace rpcz
#endif
