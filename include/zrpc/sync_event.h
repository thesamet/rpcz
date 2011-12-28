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

#ifndef RPCZ_SYNC_EVENT_H
#define RPCZ_SYNC_EVENT_H

#include <boost/thread/thread.hpp>
#include "rpcz/macros.h"

namespace rpcz {

// SyncEvent provides a mechanism for threads to wait for an event.
class SyncEvent {
 public:
  SyncEvent();

  // Blocks the current thread until another thread calls Signal().
  void Wait();

  // Signals that the event has occured. All threads that called Wait() are
  // released.
  void Signal();

 private:
  bool ready_;
  boost::mutex mu_;
  boost::condition_variable cond_;
  DISALLOW_COPY_AND_ASSIGN(SyncEvent);
};

}  // namespace rpcz
#endif
