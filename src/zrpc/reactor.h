// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_REACTOR_H
#define ZRPC_REACTOR_H

#include <utility>
#include <vector>
#include "zrpc/macros.h"

namespace zmq {
class socket_t; 
}  // namespace zmq

namespace zrpc {

class StoppingCondition {
 public:
  virtual bool ShouldStop() {
    return false;
  }
};

class Reactor {
  public:
    Reactor();
    ~Reactor();

    void AddSocket(zmq::socket_t* socket, Closure* callback);

    void LoopUntil(StoppingCondition* stopping_condition);

    void SetShouldQuit();
    
  private:
    bool should_quit_;
    bool is_dirty_;
    std::vector<std::pair<zmq::socket_t*, Closure*> > sockets_;
};
}  // namespace
#endif
