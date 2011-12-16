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

#ifndef ZRPC_REACTOR_H
#define ZRPC_REACTOR_H

#include <utility>
#include <map>
#include <vector>
#include <zmq.hpp>
#include "zrpc/macros.h"

namespace zrpc {

class Closure;
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

  int LoopUntil(StoppingCondition* stopping_condition);

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
