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

#include "zrpc/function_server.h"
#include "boost/bind.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/tss.hpp"
#include "zmq.hpp"

#include <string>
#include "zrpc/callback.h"
#include "zrpc/logging.h"
#include "zrpc/reactor.h"
#include "zrpc/zmq_utils.h"

namespace zrpc {

namespace {
const static char *kQuit = "QUIT";
const static char *kCall = "CALL";

void DeviceThreadEntryPoint(zmq::context_t* context,
                            int device_type,
                            int frontend_type,
                            const std::string& frontend_endpoint,
                            int backend_type,
                            const std::string& backend_endpoint,
                            const std::string& sync_endpoint) {
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
    zmq::context_t* context, const std::string& backend,
    const std::string& pubsub_backend,
    const std::string& sync_endpoint,
    FunctionServer::ThreadInitFunc thread_init);
}  // unnamed namespace

template <typename Callable>
boost::thread* CreateThread(Callable callable) {
  boost::thread* t = new boost::thread(callable);
  return t;
}

FunctionServer::FunctionServer(
    zmq::context_t* context, int nthreads,
    ThreadInitFunc thread_init_func)
  : context_(context),
    nthreads_(nthreads) {
  Init(thread_init_func);
}

FunctionServer::~FunctionServer() {
  Quit();
  worker_threads_->join_all();
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
    FunctionServer::ThreadInitFunc thread_init_func) {
  thread_context_.reset(new boost::thread_specific_ptr<
                        internal::ThreadContext>());
  worker_threads_.reset(new boost::thread_group());
  device_threads_.reset(new boost::thread_group());
  frontend_endpoint_ = "inproc://" +
      boost::lexical_cast<std::string>(this) + ".frontend";
  backend_endpoint_ = "inproc://" +
      boost::lexical_cast<std::string>(this) + ".backend";
  pubsub_frontend_endpoint_ = "inproc://" +
      boost::lexical_cast<std::string>(this) + ".all.frontend";
  pubsub_backend_endpoint_ = "inproc://" +
      boost::lexical_cast<std::string>(this) + ".all.backend";
  std::string sync_endpoint = "inproc://" +
      boost::lexical_cast<std::string>(this) + ".sync";
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
                        sync_endpoint, thread_init_func)));
  }
  for (int i = 0; i < nthreads_; ++i) {
    ready_sync.recv(&msg);
  }
}

void FunctionServer::Reply(MessageVector* routes,
                           MessageVector* reply) {
  internal::ThreadContext* context = thread_context_->get();
  if (context != NULL) {
    if (reply) {
      WriteVectorsToSocket(context->app_socket, *routes, *reply);
    }
  } else {
    LOG(ERROR) << "Reply() has been called in a non function-server thread.";
  }
  delete routes;
}

class FunctionServerThread {
 public:
  explicit FunctionServerThread(
      FunctionServer* function_server,
      zmq::context_t* context,
      zmq::socket_t* app_socket,
      zmq::socket_t* sub_socket,
      FunctionServer::ThreadInitFunc thread_init) :
  function_server_(function_server) {
    thread_context_ = new internal::ThreadContext;
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
    thread_context_->reactor->Loop();
  }

  ~FunctionServerThread() {
    delete thread_context_->reactor;
  }

 private:
  void HandleSubscribeSocket() {
    MessageVector data;
    CHECK(ReadMessageToVector(thread_context_->sub_socket, &data));
    std::string command(MessageToString(data[0]));
    if (command == kQuit) {
      thread_context_->reactor->SetShouldQuit();
    } else if (command == kCall) {
      InterpretMessage<Closure*>(data[1])->Run();
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
  }

  void HandleAppSocket() {
    MessageVector* routes = new MessageVector;
    MessageVector request;
    CHECK(ReadMessageToVector(thread_context_->app_socket, routes, &request));
    CHECK(request.size() == 1);
    FunctionServer::ReplyFunction reply_function =
        FunctionServer::ReplyFunction(
            boost::bind(
                &FunctionServer::Reply,
                function_server_,
                routes,
                _1));
    InterpretMessage<FunctionServer::HandlerFunction*>(request[0])->Run(
        reply_function);
  }

  // owned by FunctionServer.thread_context_;
  internal::ThreadContext* thread_context_;
  FunctionServer* function_server_;
  DISALLOW_COPY_AND_ASSIGN(FunctionServerThread);
};

namespace {
void FunctionServerThreadEntryPoint(
    FunctionServer* fs,
    zmq::context_t* context,
    const std::string& backend,
    const std::string& pubsub_backend,
    const std::string& sync_endpoint,
    FunctionServer::ThreadInitFunc thread_init) {
  zmq::socket_t* app = new zmq::socket_t(*context, ZMQ_DEALER);
  app->connect(backend.c_str());
  zmq::socket_t* pubsub = new zmq::socket_t(*context, ZMQ_SUB);
  pubsub->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
  pubsub->connect(pubsub_backend.c_str());

  zmq::socket_t sync_socket(*context, ZMQ_PUSH);
  sync_socket.connect(sync_endpoint.c_str());
  SendString(&sync_socket, "");

  FunctionServerThread fst(fs, context, app, pubsub, thread_init);
  fst.Start();
}
}  // unnamed namespace
}  // namespace zrpc
