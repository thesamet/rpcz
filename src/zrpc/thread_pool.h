// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

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
