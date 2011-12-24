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

#ifndef ZRPC_FUNCTION_SERVER_H
#define ZRPC_FUNCTION_SERVER_H

#include <boost/function.hpp>
#include <zmq.hpp>
#include "zrpc/macros.h"
#include "zrpc/zmq_utils.h"

namespace boost {
class thread;
class thread_group;
template <typename T>
class thread_specific_ptr;
}  // namespace boost
namespace zmq {
  class context_t;
  class socket_t;
}  // namespace zmq

namespace zrpc {
// A multithreaded function executor.
class FunctionServerController;
class Reactor;

namespace internal {
  struct ThreadContext {
    zmq::context_t* zmq_context;
    zmq::socket_t* app_socket;
    zmq::socket_t* sub_socket;
    Reactor* reactor;
  };
}  // namespace FunctionServer

class FunctionServer {
 public:
  typedef boost::function<void(MessageVector*)> ReplyFunction;
  typedef boost::function<void(FunctionServer*, internal::ThreadContext*)>
      ThreadInitFunc;

  class HandlerFunction {
    typedef boost::function<void(ReplyFunction)> FunctionType;

   public:
    template <typename T>
    HandlerFunction(T function) : function_(FunctionType(function)) {}

    inline void Run(ReplyFunction reply_function) {
      function_(reply_function);
      delete this;
    }

   private:
    FunctionType function_;
  };

  // Constructs an EventManager with nthreads worker threads. 
  // By the time the constructor returns all the threads are running.
  // The actual number of threads that are started may be larger than nthreads
  // by a small constant (such as 2), for internal worker threads.
  FunctionServer(zmq::context_t* context, int nthreads,
                 ThreadInitFunc thread_init_func);

  ~FunctionServer();

  // Returns a dealer socket connected to the frontend of this function server.
  zmq::socket_t* GetConnectedSocket() const;

  // Sends a function object to the socket, for later execution by the function
  // server.
  template <typename T>
  inline static void AddFunction(zmq::socket_t* socket, T function) {
    SendEmptyMessage(socket, ZMQ_SNDMORE);
    SendPointer(socket, new HandlerFunction(function), 0);
  }

 private:
  void Init(ThreadInitFunc thread_init_func);

  void Reply(MessageVector* routes,
             MessageVector* reply);

  void Quit();

  zmq::context_t* context_;
  int nthreads_;

  // Local thread-data for each worker thread.
  scoped_ptr<boost::thread_specific_ptr<internal::ThreadContext> >
      thread_context_;

  scoped_ptr<boost::thread_group> worker_threads_;
  scoped_ptr<boost::thread_group> device_threads_;
  std::string frontend_endpoint_;
  std::string pubsub_frontend_endpoint_;
  std::string backend_endpoint_;
  std::string pubsub_backend_endpoint_;
  friend class FunctionServerThread;
  DISALLOW_COPY_AND_ASSIGN(FunctionServer);
};
}  // namespace zrpc
#endif
