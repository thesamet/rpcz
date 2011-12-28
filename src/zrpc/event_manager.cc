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

#include "rpcz/event_manager.h"
#include "boost/thread/thread.hpp"
#include "boost/thread/tss.hpp"
#include "zmq.hpp"
#include "rpcz/callback.h"
#include "rpcz/function_server.h"
#include "rpcz/logging.h"
#include "rpcz/zmq_utils.h"

namespace rpcz {

void ClosureRunner(Closure* closure, FunctionServer::ReplyFunction reply) {
  closure->Run();
  reply(NULL);
}

// EventManagerController is stored in thread-local storage and contains
// a socket that is connected to the event manager frontend. This allows a way
// for any thread to talk to the EventManager.
class EventManagerController {
 public:
  EventManagerController(zmq::socket_t* socket)
      : socket_(socket) {}

  inline void Add(Closure* closure) {
    FunctionServer::AddFunction(
        socket_.get(), 
        bind(ClosureRunner, closure, _1));
  }

 private:
  scoped_ptr<zmq::socket_t> socket_;
};

EventManager::EventManager(
    zmq::context_t* context, int nthreads) 
  : function_server_(NULL),
    controller_(new boost::thread_specific_ptr<EventManagerController>) {
  InitFunctionServer(context, nthreads);
};

EventManager::EventManager(
    FunctionServer* fs)
  : function_server_(fs),
    controller_(new boost::thread_specific_ptr<EventManagerController>) {
}

void EventManager::InitFunctionServer(zmq::context_t* context, int nthreads) {
  function_server_ = new FunctionServer(
      context, nthreads,
      FunctionServer::ThreadInitFunc());
}

EventManager::~EventManager() {
  delete function_server_;
}

EventManagerController* EventManager::GetController() const {
  EventManagerController* controller = controller_->get();
  if (controller == NULL) {
    controller = new EventManagerController(
        function_server_->GetConnectedSocket());
    controller_->reset(controller);
  }
  return controller;
}

void EventManager::Add(Closure* closure) {
  GetController()->Add(closure);
}
}  // namespace rpcz
