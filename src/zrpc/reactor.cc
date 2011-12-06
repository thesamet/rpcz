// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <vector>
#include "glog/logging.h"
#include "zrpc/macros.h"
#include "zrpc/reactor.h"
#include "zmq.hpp"

namespace zrpc {

Reactor::Reactor() : should_quit_(false) {
};

Reactor::~Reactor() {
  DeleteContainerPairPointers(sockets_.begin(), sockets_.end());
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
  for (int i = 0; i < sockets.size(); ++i) {
    zmq::socket_t& socket = *sockets[i].first;
    zmq::pollitem_t pollitem = {socket, 0, ZMQ_POLLIN, 0};
    (*pollitems)[i] = pollitem;
  }
}
}  // namespace

void Reactor::LoopUntil(StoppingCondition* stop_condition) {
  std::vector<zmq::pollitem_t> pollitems;
  while (!should_quit_ && (stop_condition == NULL ||
                           !stop_condition->ShouldStop())) {
    if (is_dirty_) {
      RebuildPollItems(sockets_, &pollitems);
      is_dirty_ = false;
    }
    int rc = zmq::poll(&pollitems[0], pollitems.size(), 1000000);
    CHECK_NE(rc, -1);
    for (int i = 0; i < pollitems.size(); ++i) {
      if (!pollitems[i].revents & ZMQ_POLLIN) {
        continue;
      }
      pollitems[i].revents = 0;
      sockets_[i].second->Run();
    }
  }
}

void Reactor::SetShouldQuit() {
  should_quit_ = true;
}
}  // namespace zrpc
