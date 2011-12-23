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

#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/tss.hpp>
#include <zmq.hpp>
#include "glog/logging.h"
#include "zrpc/base/stringprintf.h"
#include "zrpc/callback.h"
#include "zrpc/function_server.h"
#include "zrpc/reactor.h"
#include "zrpc/zmq_utils.h"

namespace zrpc {

namespace {
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
  zmq::socket_t sync_socket(*context, ZMQ_PUSH);
  sync_socket.connect(sync_endpoint.c_str());
  SendString(&sync_socket, "");

  zmq_device(device_type, frontend, backend);
  frontend.close();
  backend.close();
}

void FunctionServerThreadEntryPoint(
    FunctionServer* fs,
    zmq::context_t* context, const string backend,
    const string pubsub_backend,
    const string sync_endpoint,
    FunctionServer::HandlerFunction handler_function,
    FunctionServer::ThreadInitFunc thread_init);
}  // unnamed namespace

template <typename Callable>
boost::thread* CreateThread(Callable callable) {
  boost::thread* t = new boost::thread(callable);
  return t;
}

FunctionServer::FunctionServer(
    zmq::context_t* context, int nthreads,
    HandlerFunction handler_function,
    ThreadInitFunc thread_init_func)
  : context_(context),
    nthreads_(nthreads) {
  Init(handler_function, thread_init_func);
}

FunctionServer::~FunctionServer() {
  Quit();
  VLOG(2) << "Waiting for FunctionServerThreads to quit.";
  worker_threads_->join_all();
  VLOG(2) << "FunctionServerThreads finished.";
}

zmq::socket_t* FunctionServer::GetConnectedSocket() const {
  zmq::socket_t* socket = CHECK_NOTNULL(
      new zmq::socket_t(*context_, ZMQ_DEALER));
  socket->connect(frontend_endpoint_.c_str());
  return socket;
}

void FunctionServer::Quit() {
  zmq::socket_t pubsub_socket(*context_, ZMQ_PUB);
  pubsub_socket.connect(pubsub_frontend_endpoint_.c_str());
  SendString(&pubsub_socket, kQuit, 0);
  pubsub_socket.close();
}

void FunctionServer::Init(
    FunctionServer::HandlerFunction handler_function,
    FunctionServer::ThreadInitFunc thread_init_func) {
  thread_context_.reset(new boost::thread_specific_ptr<ThreadContext>());
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
      CreateThread(
          boost::bind(&DeviceThreadEntryPoint,
                      context_,
                      ZMQ_QUEUE,
                      ZMQ_ROUTER, frontend_endpoint_,
                      ZMQ_DEALER, backend_endpoint_,
                      sync_endpoint)));
  device_threads_->add_thread(
      CreateThread(
          boost::bind(&DeviceThreadEntryPoint,
                               context_,
                               ZMQ_FORWARDER,
                               ZMQ_SUB, pubsub_frontend_endpoint_,
                               ZMQ_PUB, pubsub_backend_endpoint_,
                               sync_endpoint)));
  zmq::message_t msg;
  ready_sync.recv(&msg);
  ready_sync.recv(&msg);
  for (int i = 0; i < nthreads_; ++i) {
    worker_threads_->add_thread(
        CreateThread(
            boost::bind(&FunctionServerThreadEntryPoint,
                        this,
                        context_, backend_endpoint_,
                        pubsub_backend_endpoint_,
                        sync_endpoint, handler_function, thread_init_func)));
  }
  for (int i = 0; i < nthreads_; ++i) {
    ready_sync.recv(&msg);
  }
  VLOG(2) << "FunctionServer is up.";
}

void FunctionServer::Reply(MessageVector* routes,
                           MessageVector* request,
                           const MessageVector* reply) {
  ThreadContext* context = thread_context_->get();
  if (context != NULL) {
    if (reply) {
      WriteVectorsToSocket(context->app_socket, *routes, *reply);
    }
  } else {
    LOG(ERROR) << "Reply() has been called in a non function-server thread.";
  }
  delete request;
  delete routes;
}

class FunctionServerThread {
 public:
  explicit FunctionServerThread(
      FunctionServer* function_server,
      zmq::context_t* context,
      zmq::socket_t* app_socket,
      zmq::socket_t* sub_socket,
      FunctionServer::HandlerFunction handler_function,
      FunctionServer::ThreadInitFunc thread_init) :
  function_server_(function_server),
  handler_function_(handler_function) {
    thread_context_ = new FunctionServer::ThreadContext;
    thread_context_->zmq_context = context;
    thread_context_->app_socket = app_socket;
    thread_context_->sub_socket = sub_socket;
    thread_context_->reactor = new Reactor();
    function_server_->thread_context_->reset(thread_context_);
    if (!thread_init.empty()) {
      thread_init(function_server, thread_context_);
    }
  }

  void Start() {
    thread_context_->reactor->AddSocket(
        thread_context_->app_socket, NewPermanentCallback(
            this, &FunctionServerThread::HandleAppSocket));
    thread_context_->reactor->AddSocket(
        thread_context_->sub_socket, NewPermanentCallback(
            this, &FunctionServerThread::HandleSubscribeSocket));
    thread_context_->reactor->LoopUntil(NULL);
  }

  ~FunctionServerThread() {
    delete thread_context_->reactor;
  }

 private:
  void HandleSubscribeSocket() {
    MessageVector data;
    CHECK(ReadMessageToVector(thread_context_->sub_socket, &data));
    std::string command(MessageToString(data[0]));
    VLOG(2)<<"  Got PUBSUB command: " << command;
    if (command == kQuit) {
      thread_context_->reactor->SetShouldQuit();
    } else if (command == kCall) {
      InterpretMessage<Closure*>(*data[1])->Run();
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
  }

  void HandleAppSocket() {
    MessageVector* routes = new MessageVector;
    MessageVector* request = new MessageVector;
    CHECK(ReadMessageToVector(thread_context_->app_socket, routes, request));
    FunctionServer::ReplyFunction reply_function =
        FunctionServer::ReplyFunction(
            boost::bind(
                &FunctionServer::Reply,
                function_server_,
                routes,
                request,
                _1));
    handler_function_(request, reply_function);
  }

  // owned by FunctionServer.thread_context_;
  FunctionServer::ThreadContext* thread_context_;
  FunctionServer::HandlerFunction handler_function_;
  FunctionServer* function_server_;
  DISALLOW_COPY_AND_ASSIGN(FunctionServerThread);
};

namespace {
void FunctionServerThreadEntryPoint(
    FunctionServer* fs,
    zmq::context_t* context, const string backend,
    const string pubsub_backend,
    const string sync_endpoint,
    FunctionServer::HandlerFunction handler_function,
    FunctionServer::ThreadInitFunc thread_init) {
  zmq::socket_t* app = new zmq::socket_t(*context, ZMQ_DEALER);
  app->connect(backend.c_str());
  zmq::socket_t* pubsub = new zmq::socket_t(*context, ZMQ_SUB);
  pubsub->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
  pubsub->connect(pubsub_backend.c_str());

  zmq::socket_t sync_socket(*context, ZMQ_PUSH);
  sync_socket.connect(sync_endpoint.c_str());
  SendString(&sync_socket, "");

  FunctionServerThread fst(fs, context, app, pubsub, handler_function,
                           thread_init);
  fst.Start();
}
}  // unnamed namespace
}  // namespace zrpc
