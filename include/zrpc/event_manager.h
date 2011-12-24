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

#ifndef ZRPC_EVENT_MANAGER_H
#define ZRPC_EVENT_MANAGER_H

#include <vector>
#include <string>
#include "zrpc/macros.h"

namespace zmq {
  class context_t;
  class socket_t;
}  // namespace zmq
namespace boost {
template <typename T>
class thread_specific_ptr;
}  // namespace boost

namespace zrpc {

class Closure;
class EventManagerController;
class FunctionServer;

// EventManager is a multithreaded closure runner.
//
// Usage:
//   zmq::context_t context(1);
//   EventManager em(&context, 10);
//   em.Add(NewCallback(&MyFunction, arg1, arg2));
class EventManager {
 public:
  // Constructs an EventManager with nthreads worker threads. 
  // By the time the constructor returns all the threads are running.
  // The actual number of threads that are started may be larger than nthreads
  // by a small constant (such as 2), for internal worker threads.
  // Does not own the provided context.
  EventManager(zmq::context_t* context, int nthreads);

  virtual ~EventManager();

  // Adds a closure to the event manager. The closure will be ran by one of 
  // the EventManager threads.
  virtual void Add(Closure* c);

 private:
  zmq::context_t* context_;
  FunctionServer* function_server_;

  EventManager(FunctionServer* function_server);
  EventManagerController* GetController() const;

  void InitFunctionServer(zmq::context_t* context, int nthreads);

  FunctionServer* GetFunctionServer() { return function_server_; }

  // Lets any thread in the program have a single EventManagerController.
  scoped_ptr<boost::thread_specific_ptr<EventManagerController> > controller_;

  friend class EventManagerThread;
  friend class ConnectionThreadContext;
  friend class ConnectionManager;
  friend class Server;
  DISALLOW_COPY_AND_ASSIGN(EventManager);
};
}  // namespace zrpc
#endif
