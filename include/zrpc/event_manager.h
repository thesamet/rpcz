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
}  // namespace zmq
namespace boost {
class thread;
class thread_group;
template <typename T>
class thread_specific_ptr;
}  // namespace boost

namespace zrpc {

class Closure;
class EventManagerController;

boost::thread* CreateThread(Closure *closure);

// EventManager is a multithreaded closure runner.
//
// Usage:
//   zmq::context_t context(1);
//   EventManager em(&context, 10);
//   em.Add(NewCallback(&MyFunction, arg1, arg2));

class EventManager {
 public:
  // Starts an EventManager with nthreads threads.
  // Does not own the provided context.
  EventManager(zmq::context_t* context, int nthreads);

  virtual ~EventManager();

  // Adds a closure to the event manager. The closure will be ran by one of 
  // the EventManager threads.
  virtual void Add(Closure* c);

  // Adds a closure to the event manager that is going to be ran by each of
  // this event manager threads. It is therefore essential to use a non
  // self-deleting closure, like the one returned by NewPermanentCallback.
  // The function returns after all the threads have executed the closure.
  // It is the caller responsibility to deallocate the closure.
  virtual void Broadcast(Closure* c);

 private:
  void Init();
  EventManagerController* GetController() const;

  zmq::context_t* context_;
  int nthreads_;
  bool owns_context_;
  scoped_ptr<boost::thread_specific_ptr<EventManagerController> > controller_;
  scoped_ptr<boost::thread_group> worker_threads_;
  scoped_ptr<boost::thread_group> device_threads_;
  std::string frontend_endpoint_;
  std::string pubsub_frontend_endpoint_;
  std::string backend_endpoint_;
  std::string pubsub_backend_endpoint_;
  DISALLOW_COPY_AND_ASSIGN(EventManager);
};

}  // namespace zrpc
#endif
