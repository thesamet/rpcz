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
#include "zrpc/macros.h"

namespace zmq {
  class context_t;
}  // namespace zmq

namespace zrpc {

class EventManager {
 public:
  EventManager(zmq::context_t* context, int nthreads);

 private:
  void Init();
  zmq::context_t* context_;
  int nthreads_;
  std::vector<pthread_t> threads_;
  pthread_t worker_device_thread_;
  pthread_t pubsub_device_thread_;
  pthread_key_t controller_key_;
  bool owns_context_;
  friend class Connection;
  friend class ConnectionImpl;
  DISALLOW_COPY_AND_ASSIGN(EventManager);
};

}  // namespace zrpc
#endif
