// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_REACTOR_H
#define ZRPC_REACTOR_H

#include <utility>
#include <map>
#include <vector>
#include <zmq.hpp>
#include "zrpc/macros.h"

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

    void RunClosureAt(uint64 timestamp, Closure *callback);

    void LoopUntil(StoppingCondition* stopping_condition);

    void SetShouldQuit();
    
  private:
    long ProcessClosureRunMap();

    bool should_quit_;
    bool is_dirty_;
    std::vector<std::pair<zmq::socket_t*, Closure*> > sockets_;
    std::vector<zmq::pollitem_t> pollitems_;
    typedef std::map<uint64, std::vector<Closure*> > ClosureRunMap;
    ClosureRunMap closure_run_map_;
    DISALLOW_COPY_AND_ASSIGN(Reactor);
};
}  // namespace
#endif
