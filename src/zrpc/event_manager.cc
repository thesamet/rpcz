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

#include <boost/thread/thread.hpp>
#include <boost/thread/tss.hpp>
#include <zmq.hpp>
#include "glog/logging.h"
#include "zrpc/base/stringprintf.h"
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

const static char *kQuit = "QUIT";
const static char *kCall = "CALL";

void DeviceThreadEntryPoint(zmq::context_t* context,
                            int device_type,
                            int frontend_type,
                            const string frontend_endpoint,
                            int backend_type,
                            const string backend_endpoint,
                            const string sync_endpoint) {
  zmq::socket_t frontend(*context, frontend_type);
  zmq::socket_t backend(*context, backend_type);
  if (frontend_type == ZMQ_SUB) {
    frontend.setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
  }
  frontend.bind(frontend_endpoint.c_str());
  backend.bind(backend_endpoint.c_str());
  LOG(INFO)<<"Devicing " << frontend_endpoint << " " << backend_endpoint;
  zmq::socket_t sync_socket(*context, ZMQ_PUSH);
  sync_socket.connect(sync_endpoint.c_str());
  SendString(&sync_socket, "");

  zmq_device(device_type, frontend, backend);
  frontend.close();
  backend.close();
  LOG(INFO) << "Device terminated.";
}

void EventManagerThreadEntryPoint(
    zmq::context_t* context, const string backend,
    const string pubsub_backend,
    const string sync_endpoint);
}  // unnamed namespace

class ClosureRunnerFunction {
 public:
  void operator()(Closure *closure) {
    closure->Run();
  }
};

boost::thread* CreateThread(Closure *closure) {
  ClosureRunnerFunction func;
  boost::thread* t = new boost::thread(func, closure);
  return t;
}

// EventManagerController is stored in thread-local storage and contains
// a socket that is connected to the event manager frontend. This allows a way
// for any thread to talk to the EventManager.
class EventManagerController {
 public:
  EventManagerController(zmq::context_t* context,
                         zmq::socket_t* socket, zmq::socket_t* pubsub_socket)
      : context_(context), socket_(socket), pubsub_socket_(pubsub_socket) {}

  ~EventManagerController() {
    LOG(INFO) << "EventManagerController";
    delete socket_;
    delete pubsub_socket_;
  }

  void Quit() {
    SendString(pubsub_socket_, kQuit, 0);
  }

  inline void Add(Closure* closure) {
    SendEmptyMessage(socket_, ZMQ_SNDMORE);
    SendPointer(socket_, closure);
  }

  inline void Broadcast(Closure* closure, int thread_count) {
    SendString(pubsub_socket_, kCall, ZMQ_SNDMORE);
    SendPointer(pubsub_socket_, closure, 0);
  }

 private:
  zmq::context_t* context_;
  zmq::socket_t* socket_;
  zmq::socket_t* pubsub_socket_;
};


EventManager::EventManager(
    zmq::context_t* context, int nthreads)
  : context_(context),
    nthreads_(nthreads),
    owns_context_(false) {
  Init();
};

EventManager::~EventManager() {
  EventManagerController* controller = GetController();
  controller->Quit();
  controller_->reset(NULL);
  VLOG(2) << "Waiting for EventManagerThreads to quit.";
  worker_threads_->join_all();
  VLOG(2) << "EventManagerThreads finished.";
  if (owns_context_) {
    delete context_;
  }
  LOG(INFO) << "Destructed.";
}

EventManagerController* EventManager::GetController() const {
  EventManagerController* controller = controller_->get();
  if (controller == NULL) {
    zmq::socket_t* socket = new zmq::socket_t(*context_, ZMQ_DEALER);
    socket->connect(frontend_endpoint_.c_str());
    zmq::socket_t* pubsub_socket = new zmq::socket_t(*context_, ZMQ_PUB);
    pubsub_socket->connect(pubsub_frontend_endpoint_.c_str());
    controller = new EventManagerController(context_, socket, pubsub_socket);
    controller_->reset(controller);
  }
  return controller;
}

void EventManager::Add(Closure* closure) {
  GetController()->Add(closure);
}

void EventManager::Broadcast(Closure* closure) {
  GetController()->Broadcast(closure, nthreads_);
}

void EventManager::Init() {
  controller_.reset(new boost::thread_specific_ptr<EventManagerController>());
  worker_threads_.reset(new boost::thread_group());
  device_threads_.reset(new boost::thread_group());
  frontend_endpoint_ = StringPrintf("inproc://%p.frontend", this);
  backend_endpoint_ = StringPrintf("inproc://%p.backend", this);
  pubsub_frontend_endpoint_ = StringPrintf("inproc://%p.all.frontend", this);
  pubsub_backend_endpoint_ = StringPrintf("inproc://%p.all.backend", this);
  string sync_endpoint = StringPrintf("inproc://%p.sync", this);
  zmq::socket_t ready_sync(*context_, ZMQ_PULL);
  ready_sync.bind(sync_endpoint.c_str());
  device_threads_->add_thread(
      CreateThread(NewCallback(&DeviceThreadEntryPoint,
                               context_,
                               ZMQ_QUEUE,
                               ZMQ_ROUTER, frontend_endpoint_,
                               ZMQ_DEALER, backend_endpoint_,
                               sync_endpoint)));
  device_threads_->add_thread(
      CreateThread(NewCallback(&DeviceThreadEntryPoint,
                               context_,
                               ZMQ_FORWARDER,
                               ZMQ_SUB, pubsub_frontend_endpoint_,
                               ZMQ_PUB, pubsub_backend_endpoint_,
                               sync_endpoint)));
  zmq::message_t msg;
  ready_sync.recv(&msg);
  ready_sync.recv(&msg);
  LOG(INFO)<<"Starting threads";
  for (int i = 0; i < nthreads_; ++i) {
    worker_threads_->add_thread(
        CreateThread(NewCallback(&EventManagerThreadEntryPoint,
                                 context_, backend_endpoint_,
                                 pubsub_backend_endpoint_,
                                 sync_endpoint)));
  }
  for (int i = 0; i < nthreads_; ++i) {
    ready_sync.recv(&msg);
  }
  VLOG(2) << "EventManager is up.";
}

class EventManagerThread {
 public:
  explicit EventManagerThread(zmq::context_t* context,
                              zmq::socket_t* app_socket,
                              zmq::socket_t* sub_socket)
      : reactor_(), context_(context),
        app_socket_(app_socket),
        sub_socket_(sub_socket) {}

  void Start() {
    reactor_.AddSocket(app_socket_, NewPermanentCallback(
            this, &EventManagerThread::HandleAppSocket));

    reactor_.AddSocket(sub_socket_, NewPermanentCallback(
            this, &EventManagerThread::HandleSubscribeSocket));
    reactor_.LoopUntil(NULL);
  }

  ~EventManagerThread() {
  }

 private:
  void HandleSubscribeSocket() {
    MessageVector data;
    CHECK(ReadMessageToVector(sub_socket_, &data));
    std::string command(MessageToString(data[0]));
    VLOG(2)<<"  Got PUBSUB command: " << command;
    if (command == kQuit) {
      reactor_.SetShouldQuit();
    } else if (command == kCall) {
      InterpretMessage<Closure*>(*data[1])->Run();
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
  }

  void HandleAppSocket() {
    MessageVector routes;
    MessageVector data;
    CHECK(ReadMessageToVector(app_socket_, &routes, &data));
    // std::string command(MessageToString(data[0]));
    InterpretMessage<Closure*>(*data[0])->Run();
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
  zmq::context_t* context_;
  zmq::socket_t* app_socket_;
  zmq::socket_t* sub_socket_;
  DISALLOW_COPY_AND_ASSIGN(EventManagerThread);
};

namespace {
void EventManagerThreadEntryPoint(
    zmq::context_t* context, const string backend,
    const string pubsub_backend,
    const string sync_endpoint) {
  zmq::socket_t* app = new zmq::socket_t(*context, ZMQ_DEALER);
  app->connect(backend.c_str());
  zmq::socket_t* pubsub = new zmq::socket_t(*context, ZMQ_SUB);
  pubsub->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
  pubsub->connect(pubsub_backend.c_str());

  zmq::socket_t sync_socket(*context, ZMQ_PUSH);
  sync_socket.connect(sync_endpoint.c_str());
  SendString(&sync_socket, "");

  EventManagerThread emt(context, app, pubsub);
  emt.Start();
  LOG(INFO) << "EventManagerThread terminated.";
}
}  // unnamed namespace
}  // namespace zrpc
