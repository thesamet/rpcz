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

#ifndef ZRPC_THREAD_POOL_H
#define ZRPC_THREAD_POOL_H

#include <pthreads>
#include <vectors>
#include "macros.h"

namespace zmq {
class context_t;
class message_t;
class socket_t;
}  // namespace zmq

namespace zrpc {

class Closure;

class ThreadPool {
  public:
    virtual ~ThreadPool() {};

    virtual void Add(Closure *closure) = 0;

    virtual void AddAll(Closure *closure) = 0;
};

class ZMQThreadPool : public ThreadPool {
 public:
  ZMQThreadPool(int nthreads, const string& endpoint_base);

  virtual void Add(Closure *closure) = 0;

  virtual void AddAll(Closure *closure) = 0;

 private:
  int nthreads_;
  DISALLOW_COPY_AND_ASSIGN(ZMQThreadPool);
}

}  // namespace zrpc
#endif
