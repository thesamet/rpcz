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

#include "rpcz/reactor.h"
#include <signal.h>
#include <vector>
#include "rpcz/callback.h"
#include "rpcz/clock.h"
#include "rpcz/logging.h"
#include "rpcz/macros.h"
#include "zmq.hpp"

namespace rpcz {
namespace {
static bool g_interrupted = false;
void SignalHandler(int signal_value) {
  g_interrupted = true;
}
}  // unnamed namespace

Reactor::Reactor() : should_quit_(false) {
};

Reactor::~Reactor() {
  DeleteContainerPairPointers(sockets_.begin(), sockets_.end());
  for (ClosureRunMap::const_iterator it = closure_run_map_.begin();
       it != closure_run_map_.end(); ++it) {
    DeleteContainerPointers(it->second.begin(), it->second.end());
  }
}

void Reactor::AddSocket(zmq::socket_t* socket, Closure* closure) {
  sockets_.push_back(std::make_pair(socket, closure));
  is_dirty_ = true;
}

namespace {
void RebuildPollItems(
    const std::vector<std::pair<zmq::socket_t*, Closure*> >& sockets,
    std::vector<zmq::pollitem_t>* pollitems) {
  pollitems->resize(sockets.size());
  for (size_t i = 0; i < sockets.size(); ++i) {
    zmq::socket_t& socket = *sockets[i].first;
    zmq::pollitem_t pollitem = {socket, 0, ZMQ_POLLIN, 0};
    (*pollitems)[i] = pollitem;
  }
}
}  // namespace

void Reactor::RunClosureAt(uint64 timestamp, Closure* closure) {
  closure_run_map_[timestamp].push_back(closure);
}

int Reactor::Loop() {
  while (!should_quit_ || !closure_run_map_.empty()) {
    if (is_dirty_) {
      RebuildPollItems(sockets_, &pollitems_);
      is_dirty_ = false;
    }
    long poll_timeout = ProcessClosureRunMap();

    if (should_quit_) continue;
    int rc = zmq_poll(&pollitems_[0], pollitems_.size(), poll_timeout);
    if (rc == -1) {
      int zmq_err = zmq_errno();
      CHECK_NE(zmq_err, EFAULT);
      if (zmq_err == ETERM) {
        return -1;
      }
    }
    for (size_t i = 0; i < pollitems_.size(); ++i) {
      if (!pollitems_[i].revents & ZMQ_POLLIN) {
        continue;
      }
      pollitems_[i].revents = 0;
      sockets_[i].second->Run();
    }
  }
  if (g_interrupted) {
    return -1;
  } else {
    return 0;
  }
}

long Reactor::ProcessClosureRunMap() {
  uint64 now = zclock_time();
  ClosureRunMap::iterator ub(closure_run_map_.upper_bound(now));
  for (ClosureRunMap::const_iterator it = closure_run_map_.begin();
       it != ub;
       ++it) {
    for (std::vector<Closure*>::const_iterator vit = it->second.begin();
         vit != it->second.end(); ++vit) {
      (*vit)->Run();
    }
  }
  long poll_timeout = -1;
  if (ub != closure_run_map_.end()) {
    poll_timeout = 1000 * (ub->first - now);
  }
  closure_run_map_.erase(closure_run_map_.begin(), ub);
  return poll_timeout;
}

void Reactor::SetShouldQuit() {
  should_quit_ = true;
}

void InstallSignalHandler() {
  struct sigaction action;
  action.sa_handler = SignalHandler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
}
}  // namespace rpcz
