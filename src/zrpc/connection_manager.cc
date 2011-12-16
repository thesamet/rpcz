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

#include "zrpc/connection_manager.h"

#include <pthread.h>
#include <stddef.h>
#include <unistd.h>
#include <zmq.h>
#include <zmq.hpp>
#include <algorithm>
#include <map>
#include <ostream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "google/protobuf/stubs/common.h"
#include "zrpc/rpc_channel.h"
#include "zmq_utils.h"
#include "zrpc/callback.h"
#include "zrpc/client_request.h"
#include "zrpc/clock.h"
#include "zrpc/connection_manager_controller.h"
#include "zrpc/macros.h"
#include "zrpc/reactor.h"
#include "zrpc/simple_rpc_channel.h"

namespace zrpc {
namespace {
static const uint64 kLargePrime = (1ULL << 63) - 165;
static const uint64 kGenerator = 2;

typedef uint64 EventId;

class EventIdGenerator {
 public:
  EventIdGenerator() {
    state_ = (reinterpret_cast<uint64>(this) << 32) + getpid();
  }

  EventId GetNext() {
    state_ = (state_ * kGenerator) % kLargePrime;
    return state_;
  }

 private:
  uint64 state_;
  DISALLOW_COPY_AND_ASSIGN(EventIdGenerator);
};

void* ClosureRunner(void* closure_as_void) {
  static_cast<Closure*>(closure_as_void)->Run();
  return NULL;
}

void CreateThread(Closure *closure, pthread_t* thread) {
  CHECK(pthread_create(thread, NULL, ClosureRunner, closure) == 0);
}

class ConnectionManagerThreadParams {
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
const static char *kAddRemote = "ADD_REMOTE";
const static char *kForward = "FORWARD";
const static char *kQuit = "QUIT";


class ConnectionManagerControllerImpl : public ConnectionManagerController {
 private:
  void HandleDealerSocket() {
    MessageVector messages;
    ReadMessageToVector(dealer_, &messages);
    CHECK_EQ(messages.size(), 2);
    CHECK_EQ(messages[0]->size(), 0);
    ClientRequest* client_request(
        InterpretMessage<ClientRequest*>(*messages[1]));
    CHECK_NOTNULL(client_request);
    client_request->closure->Run();
  }

 public:
  ConnectionManagerControllerImpl(zmq::context_t* context,
                             const std::string& dealer_endpoint,
                             const std::string& pub_endpoint,
                             int thread_count)
      : dealer_(new zmq::socket_t(*context, ZMQ_DEALER)),
        pub_(*context, ZMQ_PUB),
        sync_(*context, ZMQ_PULL),
        thread_count_(thread_count) {
    dealer_->connect(dealer_endpoint.c_str());
    pub_.connect(pub_endpoint.c_str());
    std::stringstream stream(std::ios_base::out);
    stream << "inproc://sync-" << this;
    sync_endpoint_name_ = stream.str();
    sync_.bind(sync_endpoint_name_.c_str());
    reactor_.AddSocket(dealer_,
                       NewPermanentCallback(
                           this,
                           &ConnectionManagerControllerImpl::HandleDealerSocket));
  }

  void AddRemoteEndpoint(Connection* connection,
                         const std::string& remote_endpoint) {
    SendString(&pub_, kAddRemote, ZMQ_SNDMORE);
    SendString(&pub_, sync_endpoint_name_, ZMQ_SNDMORE);
    SendPointer<Connection>(&pub_, connection, ZMQ_SNDMORE);
    SendString(&pub_, remote_endpoint, 0);
    for (int dummy = 0; dummy < thread_count_; ++dummy) {
      MessageVector v;
      ReadMessageToVector(&sync_, &v);
    }
  }

  void Forward(Connection* connection,
               ClientRequest* client_request,
               const MessageVector& messages) {
    SendString(dealer_, "", ZMQ_SNDMORE);
    SendString(dealer_, kForward, ZMQ_SNDMORE);
    SendPointer(dealer_, connection, ZMQ_SNDMORE);
    SendPointer<ClientRequest>(dealer_, client_request, ZMQ_SNDMORE);
    WriteVectorToSocket(dealer_, messages);
  }

  int WaitUntil(StoppingCondition *stopping_condition) {
    return reactor_.LoopUntil(stopping_condition);
  }

  void Quit() {
    SendString(&pub_, kQuit, ZMQ_SNDMORE);
    SendString(&pub_, "", 0);
  }

 private:
  Reactor reactor_;
  zmq::socket_t *dealer_;
  zmq::socket_t pub_;
  zmq::socket_t sync_;
  std::string sync_endpoint_name_;
  int thread_count_;
};

void ConnectionManagerThreadEntryPoint(
    const ConnectionManagerThreadParams params);

void DeviceThreadEntryPoint(
    const DeviceThreadParams params);

void DestroyController(void* controller) {
  LOG(INFO)<<"Destructing";
  delete static_cast<ConnectionManagerController*>(controller);
}
}  // unnamed namespace

ConnectionManager::ConnectionManager(zmq::context_t* context,
                           int nthreads) : context_(context),
                                           nthreads_(nthreads),
                                           threads_(nthreads),
                                           owns_context_(false) {
  Init();
}

ConnectionManager::ConnectionManager(int nthreads) : context_(new zmq::context_t(1)),
                                           nthreads_(nthreads),
                                           threads_(nthreads),
                                           owns_context_(true) {
  Init();
}

void ConnectionManager::Init() {
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
    ConnectionManagerThreadParams params;
    params.context = context_;
    params.dealer_endpoint = "inproc://clients.dealer";
    params.pubsub_endpoint = "inproc://clients.all";
    params.ready_sync_endpoint = "inproc://clients.ready_sync";
    Closure *cl = NewCallback(&ConnectionManagerThreadEntryPoint,
                              params);
    CreateThread(cl, &threads_[i]);
  }
  for (int i = 0; i < nthreads_; ++i) {
    ready_sync.recv(&msg);
  }
  VLOG(2) << "ConnectionManager is up.";
}

ConnectionManagerController* ConnectionManager::GetController() const {
  ConnectionManagerController* controller = static_cast<ConnectionManagerController*>(
      pthread_getspecific(controller_key_));
  if (controller == NULL) {
    controller = new ConnectionManagerControllerImpl(
      context_,
      "inproc://clients.app",
      "inproc://clients.allapp",
      GetThreadCount());
    pthread_setspecific(controller_key_, controller);
  }
  return controller;
}

ConnectionManager::~ConnectionManager() {
  ConnectionManagerController *controller = GetController();
  controller->Quit();
  delete controller;
  pthread_setspecific(controller_key_, NULL);
  VLOG(2) << "Waiting for ConnectionManagerThreads to quit.";
  for (int i = 0; i < GetThreadCount(); ++i) {
    CHECK_EQ(pthread_join(threads_[i], NULL), 0);
  }
  VLOG(2) << "ConnectionManagerThreads finished.";
  if (owns_context_) {
    delete context_;
  }
}

class ConnectionManagerThread {
 public:
  explicit ConnectionManagerThread(const ConnectionManagerThreadParams& params)
      : reactor_(), params_(params) {}

  void Start() {
    app_socket_ = new zmq::socket_t(*params_.context, ZMQ_DEALER);
    app_socket_->connect(params_.dealer_endpoint);

    zmq::socket_t* sub_socket = new zmq::socket_t(*params_.context, ZMQ_SUB);
    sub_socket->connect(params_.pubsub_endpoint);
    sub_socket->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);

    reactor_.AddSocket(app_socket_, NewPermanentCallback(
            this, &ConnectionManagerThread::HandleAppSocket));

    reactor_.AddSocket(sub_socket, NewPermanentCallback(
            this, &ConnectionManagerThread::HandleSubscribeSocket, sub_socket));

    zmq::socket_t sync_socket(*params_.context, ZMQ_PUSH);
    sync_socket.connect(params_.ready_sync_endpoint);
    SendString(&sync_socket, "");
    reactor_.LoopUntil(NULL);
  }

  ~ConnectionManagerThread() {
  }

 private:
  void AddRemoteEndpoint(
      Connection* connection,
      const std::string& remote_endpoint) {
    zmq::socket_t *socket = new zmq::socket_t(*params_.context, ZMQ_DEALER);
    connection_map_[connection] = socket;
    socket->connect(remote_endpoint.c_str());
    reactor_.AddSocket(socket, NewPermanentCallback(
            this, &ConnectionManagerThread::HandleClientSocket, socket));
  }

  void HandleTimeout(EventId event_id) {
    ClientRequestMap::iterator iter = client_request_map_.find(event_id);
    if (iter == client_request_map_.end()) {
      return;
    }
    ClientRequest*& client_request = iter->second;
    client_request->status = ClientRequest::DEADLINE_EXCEEDED;
    WriteVectorToSocket(app_socket_, client_request->return_path,
                        ZMQ_SNDMORE);
    SendPointer(app_socket_, client_request, 0);
    client_request_map_.erase(iter);
  }

  template<typename ForwardIterator>
  uint64 ForwardRemote(
      Connection* connection,
      ClientRequest* client_request,
      ForwardIterator begin,
      ForwardIterator end) {
    ConnectionMap::const_iterator it = connection_map_.find(connection);
    CHECK(it != connection_map_.end());
    EventId event_id = event_id_generator_.GetNext();
    client_request_map_[event_id] = client_request;
    if (client_request->deadline_ms != -1) {
      reactor_.RunClosureAt(
          client_request->start_time + client_request->deadline_ms,
          NewCallback(this, &ConnectionManagerThread::HandleTimeout, event_id));
    }

    zmq::socket_t* const& socket = it->second;
    SendString(socket, "", ZMQ_SNDMORE);
    SendUint64(socket, event_id, ZMQ_SNDMORE);
    for (ForwardIterator i = begin; i != end; ++i) {
      socket->send(**i, (i + 1) != end ? ZMQ_SNDMORE : 0);
    }
    return event_id;
  }

  void ReplyOK(zmq::socket_t *socket,
               const MessageVector& routes) {
    MessageVector v;
    v.push_back(StringToMessage("OK"));
    WriteVectorsToSocket(socket, routes, v);
  }

  void HandleSubscribeSocket(zmq::socket_t* sub_socket) {
    MessageVector data;
    CHECK(ReadMessageToVector(sub_socket, &data));
    std::string command(MessageToString(data[0]));
    VLOG(2)<<"  Got PUBSUB command: " << command;
    std::string sync_endpoint(MessageToString(data[1]));
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
  }

  void HandleAppSocket() {
    MessageVector routes;
    MessageVector data;
    CHECK(ReadMessageToVector(app_socket_, &routes, &data));
    std::string command(MessageToString(data[0]));
    if (command == kForward) {
      CHECK_GE(data.size(), 3);
      ClientRequest* client_request = 
          InterpretMessage<ClientRequest*>(*data[2]);
      routes.swap(client_request->return_path);
      ForwardRemote(
          InterpretMessage<Connection*>(*data[1]),
          client_request,
          data.begin() + 3, data.end());
      return;
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
  }

  void HandleClientSocket(zmq::socket_t* socket) {
    MessageVector messages;
    ReadMessageToVector(socket, &messages);
    CHECK(messages.size() >= 1);
    CHECK_EQ(messages[0]->size(), 0);
    EventId event_id(InterpretMessage<EventId>(*messages[1]));
    ClientRequestMap::iterator iter = client_request_map_.find(event_id);
    if (iter == client_request_map_.end()) {
      return;
    }
    ClientRequest*& client_request = iter->second;
    messages.erase(0);
    messages.erase(0);
    client_request->result.swap(messages);
    client_request->status = ClientRequest::DONE;
    WriteVectorToSocket(app_socket_, client_request->return_path,
                        ZMQ_SNDMORE);
    SendPointer(app_socket_, client_request, 0);
    client_request_map_.erase(iter);
  }

  Reactor reactor_;
  const ConnectionManagerThreadParams params_;
  zmq::socket_t* app_socket_;
  typedef std::map<Connection*, zmq::socket_t*> ConnectionMap;
  typedef std::map<EventId, ClientRequest*> ClientRequestMap;
  typedef std::map<uint64, EventId> DeadlineMap;
  ConnectionMap connection_map_;
  ClientRequestMap client_request_map_;
  DeadlineMap deadline_map_;
  EventIdGenerator event_id_generator_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionManagerThread);
};

class ConnectionImpl : public Connection {
 public:
  ConnectionImpl(ConnectionManager* connection_manager)
      : Connection(), connection_manager_(connection_manager) {}

  virtual RpcChannel* MakeChannel() {
    return new SimpleRpcChannel(this);
  }

  virtual void SendClientRequest(ClientRequest* client_request,
                                 const MessageVector& messages) {
    connection_manager_->GetController()->Forward(
        this, client_request, messages);
  }

  virtual int WaitUntil(StoppingCondition* condition) {
    return connection_manager_->GetController()->WaitUntil(condition);
  }

 private:
  ConnectionManager* connection_manager_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

Connection* Connection::CreateConnection(
    ConnectionManager* em, const std::string& endpoint) {
  Connection* connection = new ConnectionImpl(em);
  em->GetController()->AddRemoteEndpoint(connection, endpoint);
  return connection;
}

namespace {
void ConnectionManagerThreadEntryPoint(const ConnectionManagerThreadParams params) {
  ConnectionManagerThread emt(params);
  emt.Start();
  VLOG(2) << "ConnectionManagerThread terminated.";
}

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
}  // unnamed namespace
}  // namespace zrpc
