// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "event_manager.h"

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
#include "rpc_channel.h"
#include "zmq_utils.h"
#include "zrpc/event_manager_controller.h"
#include "zrpc/macros.h"
#include "zrpc/reactor.h"

namespace zrpc {
namespace {
static const uint64 kLargestPrime64 = 18446744073709551557ULL;
static const uint64 kGenerator = 2;

class EventIdGenerator {
 public:
  EventIdGenerator() {
    state_ = (reinterpret_cast<uint64>(this) << 32) + getpid();
  }

  uint64 GetNext() {
    state_ = (state_ * kGenerator) % kLargestPrime64;
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
const static char *kAddRemote = "ADD_REMOTE";
const static char *kForward = "FORWARD";
const static char *kQuit = "QUIT";

class EventManagerControllerImpl : public EventManagerController {
 private:
  void HandleDealerSocket() {
    std::vector<zmq::message_t*> messages;
    ReadMessageToVector(dealer_, &messages);
    CHECK_EQ(messages.size(), 2);
    CHECK_EQ(messages[0]->size(), 0);
    ClientRequest* client_request(
        InterpretMessage<ClientRequest*>(*messages[1]));
    CHECK_NOTNULL(client_request);
    client_request->closure->Run();
  }

 public:
  EventManagerControllerImpl(zmq::context_t* context,
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
                           &EventManagerControllerImpl::HandleDealerSocket));
  }

  void AddRemoteEndpoint(Connection* connection,
                         const std::string& remote_endpoint) {
    SendString(&pub_, kAddRemote, ZMQ_SNDMORE);
    SendString(&pub_, sync_endpoint_name_, ZMQ_SNDMORE);
    SendPointer<Connection>(&pub_, connection, ZMQ_SNDMORE);
    SendString(&pub_, remote_endpoint, 0);
    for (int dummy = 0; dummy < thread_count_; ++dummy) {
      std::vector<zmq::message_t*> v;
      ReadMessageToVector(&sync_, &v);
    }
  }

  void Forward(Connection* connection,
               ClientRequest* client_request,
               const std::vector<zmq::message_t*>& messages) {
    SendString(dealer_, "", ZMQ_SNDMORE);
    SendString(dealer_, kForward, ZMQ_SNDMORE);
    SendPointer(dealer_, connection, ZMQ_SNDMORE);
    SendPointer<ClientRequest>(dealer_, client_request, ZMQ_SNDMORE);
    WriteVectorToSocket(dealer_, messages);
  }

  void WaitFor(StoppingCondition *stopping_condition) {
    reactor_.LoopUntil(stopping_condition);
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

void EventManagerThreadEntryPoint(
    const EventManagerThreadParams params);

void DeviceThreadEntryPoint(
    const DeviceThreadParams params);

void DestroyController(void* controller) {
  LOG(INFO)<<"Destructing";
  delete static_cast<EventManagerController*>(controller);
}

}  // unnamed namespace

EventManager::EventManager(zmq::context_t* context,
                           int nthreads) : context_(context),
                                           nthreads_(nthreads),
                                           threads_(nthreads) {
  zmq::socket_t ready_sync(*context, ZMQ_PULL);
  CHECK(pthread_key_create(&controller_key_, &DestroyController) == 0);
  ready_sync.bind("inproc://clients.ready_sync");
  {
    DeviceThreadParams params;
    params.context = context;
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
    params.context = context;
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

  for (int i = 0; i < nthreads; ++i) {
    EventManagerThreadParams* params = new EventManagerThreadParams;
    params->context = context;
    params->dealer_endpoint = "inproc://clients.dealer";
    params->pubsub_endpoint = "inproc://clients.all";
    params->ready_sync_endpoint = "inproc://clients.ready_sync";
    Closure *cl = NewCallback(&EventManagerThreadEntryPoint,
                              *params);
    CreateThread(cl, &threads_[i]);
  }
  for (int i = 0; i < nthreads; ++i) {
    ready_sync.recv(&msg);
  }
  VLOG(2) << "EventManager is up.";
}

EventManagerController* EventManager::GetController() const {
  EventManagerController* controller = static_cast<EventManagerController*>(
      pthread_getspecific(controller_key_));
  if (controller == NULL) {
    controller = new EventManagerControllerImpl(
      context_,
      "inproc://clients.app",
      "inproc://clients.allapp",
      GetThreadCount());
    pthread_setspecific(controller_key_, controller);
  }
  return controller;
}

EventManager::~EventManager() {
  EventManagerController *controller = GetController();
  controller->Quit();
  delete controller;
  pthread_setspecific(controller_key_, NULL);
  VLOG(2) << "Waiting for EventManagerThreads to quit.";
  for (int i = 0; i < GetThreadCount(); ++i) {
    CHECK_EQ(pthread_join(threads_[i], NULL), 0);
  }
  VLOG(2) << "EventManagerThreads finished.";
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
  void AddRemoteEndpoint(
      Connection* connection,
      const std::string& remote_endpoint) {
    zmq::socket_t *socket = new zmq::socket_t(*params_.context, ZMQ_DEALER);
    connection_map_[connection] = socket;
    socket->connect(remote_endpoint.c_str());
    reactor_.AddSocket(socket, NewPermanentCallback(
            this, &EventManagerThread::HandleClientSocket, socket));
  }

  template<typename ForwardIterator>
  uint64 ForwardRemote(
    Connection* connection,
    ClientRequest* client_request,
    ForwardIterator begin,
    ForwardIterator end) {
    ConnectionMap::const_iterator it = connection_map_.find(connection);
    CHECK(it != connection_map_.end());
    uint64 event_id = event_id_generator_.GetNext();
    client_request_map_[event_id] = client_request;

    zmq::socket_t* const& socket = it->second;
    SendString(socket, "", ZMQ_SNDMORE);
    SendUint64(socket, event_id, ZMQ_SNDMORE);
    for (ForwardIterator i = begin; i != end; ++i) {
      socket->send(**i, (i + 1) != end ? ZMQ_SNDMORE : 0);
    }
    return event_id;
  }

  void ReplyOK(zmq::socket_t *socket,
               const std::vector<zmq::message_t*>& routes) {
    zmq::message_t* msg = StringToMessage("OK");
    WriteVectorsToSocket(socket, routes, std::vector<zmq::message_t*>(
            1, msg));
    delete msg;
  }

  void HandleSubscribeSocket(zmq::socket_t* sub_socket) {
    std::vector<zmq::message_t*> data;
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
    DeleteContainerPointers(data.begin(), data.end());
    if (!sync_endpoint.empty()) {
      zmq::socket_t sync_socket(*params_.context, ZMQ_PUSH);
      sync_socket.connect(sync_endpoint.c_str());
      SendString(&sync_socket, "ACK", 0);
    }
  }

  void HandleAppSocket() {
    std::vector<zmq::message_t*> routes;
    std::vector<zmq::message_t*> data;
    CHECK(ReadMessageToVector(app_socket_, &routes, &data));
    std::string command(MessageToString(data[0]));
    if (command == kForward) {
      CHECK_GE(data.size(), 3);
      ClientRequest* client_request = 
          InterpretMessage<ClientRequest*>(*data[2]);
      client_request->return_path = routes;
      ForwardRemote(
          InterpretMessage<Connection*>(*data[1]),
          client_request,
          data.begin() + 3, data.end());
      // ForwardRemote handles its own message delete or delegates it.
      return;
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
    DeleteContainerPointers(routes.begin(), routes.end());
    DeleteContainerPointers(data.begin(), data.end());
  }

  void HandleClientSocket(zmq::socket_t* socket) {
    std::vector<zmq::message_t*> messages;
    ReadMessageToVector(socket, &messages);
    CHECK(messages.size() >= 1);
    CHECK_EQ(messages[0]->size(), 0);
    uint64 event_id(InterpretMessage<uint64>(*messages[1]));
    ClientRequestMap::iterator iter = client_request_map_.find(event_id);
    if (iter == client_request_map_.end()) {
      LOG(INFO) << "Ignoring unknown incoming message.";
      DeleteContainerPointers(messages.begin(), messages.end());
      return;
    }
    ClientRequest*& client_request = iter->second;
    client_request->result.resize(messages.size() - 2);
    std::copy(messages.begin() + 2,
              messages.end(), client_request->result.begin());
    WriteVectorToSocket(app_socket_, client_request->return_path,
                        ZMQ_SNDMORE);
    SendPointer(app_socket_, client_request, 0);
    DeleteContainerPointers(messages.begin(), messages.begin() + 2);
  }

  Reactor reactor_;
  const EventManagerThreadParams params_;
  zmq::socket_t* app_socket_;
  typedef std::map<Connection*, zmq::socket_t*> ConnectionMap;
  typedef std::map<uint64, ClientRequest*> ClientRequestMap;
  ConnectionMap connection_map_;
  ClientRequestMap client_request_map_;
  EventIdGenerator event_id_generator_;
  DISALLOW_COPY_AND_ASSIGN(EventManagerThread);
};

class ConnectionImpl : public Connection {
 public:
  ConnectionImpl(EventManager* event_manager)
      : Connection(), event_manager_(event_manager) {}

  virtual RpcChannel* MakeChannel() {
    return new ZMQRpcChannel(event_manager_->GetController(), this);
  }

 private:
  EventManager* event_manager_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

Connection* Connection::CreateConnection(
    EventManager* em, const std::string& endpoint) {
  Connection* connection = new ConnectionImpl(em);
  em->GetController()->AddRemoteEndpoint(connection, endpoint);
  return connection;
}

namespace {
void EventManagerThreadEntryPoint(const EventManagerThreadParams params) {
  EventManagerThread emt(params);
  emt.Start();
  VLOG(2) << "EventManagerThread terminated.";
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
