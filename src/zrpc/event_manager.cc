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

#include <zmq.hpp>
#include "glog/logging.h"
#include "zrpc/callback.h"
#include "zrpc/event_manager.h"
#include "zrpc/reactor.h"
#include "zrpc/zmq_utils.h"

namespace zrpc {

class EventManagerThread;
namespace {
void* ClosureRunner(void* closure_as_void) {
  static_cast<Closure*>(closure_as_void)->Run();
  return NULL;
}

void CreateThread(Closure *closure, pthread_t* thread) {
  CHECK(pthread_create(thread, NULL, ClosureRunner, closure) == 0);
}

void DestroyController(void* controller) {
  LOG(INFO)<<"Destructing";
  // delete static_cast<EventManagerController*>(controller);
}

class EventManagerThreadParams {
 public:
  zmq::context_t* context;
  const char* dealer_endpoint;
  const char* pubsub_endpoint;
  const char* ready_sync_endpoint;
};

struct DeviceThreadParams {
  zmq::context_t* context;
  const char* frontend;
  const char* backend;
  const char* ready_sync;
  int frontend_type; 
  int backend_type; 
  int device_type;
};

const static char *kHello = "HELLO";

void DeviceThreadEntryPoint(const DeviceThreadParams params) {
  zmq::socket_t frontend(*params.context, params.frontend_type);
  frontend.bind(params.frontend);
  if (params.frontend_type == ZMQ_SUB) {
    frontend.setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
  }
  zmq::socket_t backend(*params.context, params.backend_type);
  backend.bind(params.backend);
  zmq::socket_t ready(*params.context, ZMQ_PUSH);
  ready.connect(params.ready_sync);
  SendString(&ready, kHello);
  VLOG(2) << "Starting device: " << params.frontend << " --> "
      << params.backend;
  zmq_device(params.device_type, frontend, backend);
  VLOG(2) << "Device terminated.";
}

void EventManagerThreadEntryPoint(const EventManagerThreadParams params);
}  // unnamed namespace

EventManager::EventManager(
    zmq::context_t* context, int nthreads)
    : context_(context),
      nthreads_(nthreads),
      threads_(nthreads),
      owns_context_(false) {
};

void EventManager::Init() {
  zmq::socket_t ready_sync(*context_, ZMQ_PULL);
  CHECK(pthread_key_create(&controller_key_, &DestroyController) == 0);
  ready_sync.bind("inproc://clients.ready_sync");
  {
    DeviceThreadParams params;
    params.context = context_;
    params.frontend = "inproc://clients.app";
    params.frontend_type = ZMQ_ROUTER;
    params.backend = "inproc://clients.dealer";
    params.backend_type = ZMQ_DEALER;
    params.ready_sync = "inproc://clients.ready_sync";
    params.device_type = ZMQ_QUEUE;
    CreateThread(NewCallback(&DeviceThreadEntryPoint,
                             params), &worker_device_thread_);
  }
  {
    DeviceThreadParams params;
    params.context = context_;
    params.frontend = "inproc://clients.allapp";
    params.frontend_type = ZMQ_SUB;
    params.backend = "inproc://clients.all";
    params.backend_type = ZMQ_PUB;
    params.ready_sync = "inproc://clients.ready_sync";
    params.device_type = ZMQ_FORWARDER;
    CreateThread(NewCallback(&DeviceThreadEntryPoint,
                             params), &pubsub_device_thread_);
  }
  zmq::message_t msg;
  ready_sync.recv(&msg);
  ready_sync.recv(&msg);

  for (int i = 0; i < nthreads_; ++i) {
    EventManagerThreadParams params;
    params.context = context_;
    params.dealer_endpoint = "inproc://clients.dealer";
    params.pubsub_endpoint = "inproc://clients.all";
    params.ready_sync_endpoint = "inproc://clients.ready_sync";
    Closure *cl = NewCallback(&EventManagerThreadEntryPoint,
                              params);
    CreateThread(cl, &threads_[i]);
  }
  for (int i = 0; i < nthreads_; ++i) {
    ready_sync.recv(&msg);
  }
  VLOG(2) << "EventManager is up.";
}

class EventManagerThread {
 public:
  explicit EventManagerThread(const EventManagerThreadParams& params)
      : reactor_(), params_(params) {}

  void Start() {
    app_socket_ = new zmq::socket_t(*params_.context, ZMQ_DEALER);
    app_socket_->connect(params_.dealer_endpoint);

    zmq::socket_t* sub_socket = new zmq::socket_t(*params_.context, ZMQ_SUB);
    sub_socket->connect(params_.pubsub_endpoint);
    sub_socket->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);

    reactor_.AddSocket(app_socket_, NewPermanentCallback(
            this, &EventManagerThread::HandleAppSocket));

    reactor_.AddSocket(sub_socket, NewPermanentCallback(
            this, &EventManagerThread::HandleSubscribeSocket, sub_socket));

    zmq::socket_t sync_socket(*params_.context, ZMQ_PUSH);
    sync_socket.connect(params_.ready_sync_endpoint);
    SendString(&sync_socket, "");
    reactor_.LoopUntil(NULL);
  }

  ~EventManagerThread() {
  }

 private:
  void HandleSubscribeSocket(zmq::socket_t* sub_socket) {
    MessageVector data;
    CHECK(ReadMessageToVector(sub_socket, &data));
    std::string command(MessageToString(data[0]));
    VLOG(2)<<"  Got PUBSUB command: " << command;
    std::string sync_endpoint(MessageToString(data[1]));
    /*
    if (command == kAddRemote) {
      CHECK_EQ(data.size(), 4);
      AddRemoteEndpoint(InterpretMessage<Connection*>(*data[2]),
                        MessageToString(data[3]));
    } else if (command == kQuit) {
      reactor_.SetShouldQuit();
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
    if (!sync_endpoint.empty()) {
      zmq::socket_t sync_socket(*params_.context, ZMQ_PUSH);
      sync_socket.connect(sync_endpoint.c_str());
      SendString(&sync_socket, "ACK", 0);
    }
    */
  }

  void HandleAppSocket() {
    MessageVector routes;
    MessageVector data;
    CHECK(ReadMessageToVector(app_socket_, &routes, &data));
    std::string command(MessageToString(data[0]));
    /*
    if (command == kForward) {
      CHECK_GE(data.size(), 3);
      RemoteResponseWrapper* remote_response_wrapper = 
          InterpretMessage<RemoteResponseWrapper*>(*data[2]);
      routes.swap(remote_response_wrapper->return_path);
      ForwardRemote(
          InterpretMessage<Connection*>(*data[1]),
          remote_response_wrapper,
          data.begin() + 3, data.end());
      return;
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
    */
  }

  Reactor reactor_;
  const EventManagerThreadParams params_;
  zmq::socket_t* app_socket_;
  DISALLOW_COPY_AND_ASSIGN(EventManagerThread);
};

namespace {
void EventManagerThreadEntryPoint(const EventManagerThreadParams params) {
  EventManagerThread emt(params);
  emt.Start();
  VLOG(2) << "EventManagerThread terminated.";
}
}
}  // namespace zrpc
