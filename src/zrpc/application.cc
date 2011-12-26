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

#include <string>
#include "zmq.hpp"
#include "zrpc/application.h"
#include "zrpc/event_manager.h"
#include "zrpc/connection_manager.h"
#include "zrpc/rpc_channel.h"
#include "zrpc/server.h"

namespace zrpc {

Application::Application() {
  Init(Options());
};

Application::Application(const Application::Options& options) {
  Init(options);
};

Application::~Application() {
  connection_manager_.reset();
  event_manager_.reset();
  if (owns_context_) {
    delete context_;
  }
}

void Application::Init(const Application::Options& options) {
  if (options.zeromq_context) {
    context_ = options.zeromq_context;
    owns_context_ = false;
  } else {
    context_ = new zmq::context_t(options.zeromq_io_threads);
    owns_context_ = true;
  }
  event_manager_.reset(new EventManager(context_,
                                        options.event_manager_threads));
  connection_manager_.reset(new ConnectionManager(
          context_,
          event_manager_.get(),
          options.connection_manager_threads));
}

RpcChannel* Application::CreateRpcChannel(const std::string& endpoint) {
  return RpcChannel::Create(
      connection_manager_->Connect(endpoint), true);
}

Server* Application::CreateServer(const std::string& endpoint) {
  zmq::socket_t* socket = new zmq::socket_t(*context_, ZMQ_ROUTER);
  socket->bind(endpoint.c_str());
  Server* server = new Server(socket, event_manager_.get(), true);
  return server;
}
}  // namespace zrpc
