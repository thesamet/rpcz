// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "event_manager.h"
#include <pthread.h>
#include <map>
#include <string>
#include <vector>
#include <zmq.hpp>
#include "glog/logging.h"
#include "google/protobuf/stubs/common.h"
#include "zmq_utils.h"

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
const static char *kCall = "CALL";
const static char *kForward = "FORWARD";
const static char *kQuit = "QUIT";

class EventManagerControllerImpl : public EventManagerController {
 public:
  EventManagerControllerImpl(zmq::context_t* context,
                             const std::string& dealer_endpoint,
                             const std::string& pub_endpoint)
      : dealer_(*context, ZMQ_REQ),
        pub_(*context, ZMQ_PUB) {
    dealer_.connect(dealer_endpoint.c_str());
    pub_.connect(pub_endpoint.c_str());
  }

  void AddRemoteEndpoint(const std::string& remote_name,
                         const std::string& remote_endpoint) {
    SendString(&pub_, kAddRemote, ZMQ_SNDMORE);
    SendPointer<Closure*>(&pub_, NULL, ZMQ_SNDMORE);
    SendString(&pub_, remote_name, ZMQ_SNDMORE);
    SendString(&pub_, remote_endpoint, 0);
  }

  void Quit() {
    SendString(&pub_, kQuit, ZMQ_SNDMORE);
    SendPointer<Closure*>(&pub_, NULL, 0);
  }

 private:
  void CheckReply() {
    zmq::message_t msg;
    dealer_.recv(&msg);
    CHECK_EQ(MessageToString(&msg), "OK");
  }

  zmq::socket_t dealer_;
  zmq::socket_t pub_;
};

void EventManagerThreadEntryPoint(
    const EventManagerThreadParams params);

void DeviceThreadEntryPoint(
    const DeviceThreadParams params);
}  // unnamed namespace


EventManager::EventManager(zmq::context_t* context,
                           int nthreads) : context_(context),
                                           nthreads_(nthreads),
                                           threads_(nthreads) {
  zmq::socket_t ready_sync(*context, ZMQ_PULL);
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
    EventManagerThreadParams params;
    params.context = context;
    params.dealer_endpoint = "inproc://clients.dealer";
    params.pubsub_endpoint = "inproc://clients.all";
    Closure *cl = NewCallback(&EventManagerThreadEntryPoint,
                              params);
    CreateThread(cl, &threads_[i]);
  }
  VLOG(2) << "EventManager is up.";
}

EventManagerController* EventManager::GetController() const {
  return new EventManagerControllerImpl(
      context_,
      "inproc://clients.app",
      "inproc://clients.allapp");
}

EventManager::~EventManager() {
  EventManagerController *controller = GetController();
  controller->Quit();
  delete controller;
  VLOG(2) << "Waiting for EventManagerThreads to quit.";
  for (int i = 0; i < GetThreadCount(); ++i) {
    CHECK_EQ(pthread_join(threads_[i], NULL), 0);
  }
  VLOG(2) << "EventManagerThreads finished.";
}

class EventManagerThread {
 public:
  explicit EventManagerThread(
      const std::string& dealer_endpoint,
      const std::string& pubsub_endpoint,
      zmq::context_t *context)
      : dealer_endpoint_(dealer_endpoint),
        pubsub_endpoint_(pubsub_endpoint),
        context_(context),
        should_quit_(false),
        is_dirty_(true) {
        }

  void Start() {
    zmq::socket_t* app_socket = new zmq::socket_t(*context_, ZMQ_ROUTER);
    app_socket->connect(dealer_endpoint_.c_str());

    zmq::socket_t* sub_socket = new zmq::socket_t(*context_, ZMQ_SUB);
    sub_socket->connect(pubsub_endpoint_.c_str());
    sub_socket->setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
    // app_socket_->setsockopt(ZMQ_IDENTITY, "appsocket", 9);

    sockets_.push_back(app_socket);
    sockets_.push_back(sub_socket);
    std::vector<zmq::pollitem_t> pollitems;
    while (!should_quit_) {
      if (is_dirty_) {
        RebuildPollItems(&pollitems);
        is_dirty_ = false;
      }
      int rc = zmq::poll(&pollitems[0], pollitems.size(), 1000000);
      for (int i = 0; i < pollitems.size(); ++i) {
        if (!pollitems[i].revents & ZMQ_POLLIN) {
          continue;
        }
        pollitems[i].revents = 0;
        if (pollitems[i].socket == *app_socket) {
          HandleAppSocket(app_socket);
        } else if (pollitems[i].socket == *sub_socket) {
          HandleSubscribeSocket(sub_socket);
        } else {
          HandleClientSocket(sockets_[i]);
        }
      }
    }
  }

  ~EventManagerThread() {
    DeleteContainerPointers(sockets_.begin(),
                            sockets_.end());
  }

  std::string dealer_endpoint_;
  std::string pubsub_endpoint_;

 private:
  void RebuildPollItems(std::vector<zmq::pollitem_t>* pollitems) {
    pollitems->resize(sockets_.size());
    for (int i = 0; i < sockets_.size(); ++i) {
      zmq::socket_t& socket = *sockets_[i];
      zmq::pollitem_t pollitem = {socket, 0, ZMQ_POLLIN, 0};
      (*pollitems)[i] = pollitem;
    }
  }

  void AddRemoteEndpoint(
      const std::string& remote_name,
      const std::string& remote_endpoint) {
    zmq::socket_t *socket = new zmq::socket_t(*context_, ZMQ_DEALER);
    connection_map_[remote_name] = socket;
    sockets_.push_back(socket);
    socket->connect(remote_endpoint.c_str());
    is_dirty_ = true;
  }

  template<typename ForwardIterator>
  uint64 ForwardRemote(
    const std::string& remote_name,
    ClientRequest* client_request,
    ForwardIterator begin,
    ForwardIterator end) {
    ConnectionMap::const_iterator it = connection_map_.find(remote_name);
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
    Closure* closure(InterpretMessage<Closure*>(*data[1]));
    if (command == kAddRemote) {
      CHECK_EQ(data.size(), 4);
      AddRemoteEndpoint(MessageToString(data[2]),
                        MessageToString(data[3]));
    } else if (command == kQuit) {
      should_quit_ = true;
    } else {
      CHECK(false) << "Got unknown command: " << command;
    }
    if (closure != NULL) {
      closure->Run();
    }
    DeleteContainerPointers(data.begin(), data.end());
  }

  void HandleAppSocket(zmq::socket_t* app_socket) {
    std::vector<zmq::message_t*> routes;
    std::vector<zmq::message_t*> data;
    CHECK(ReadMessageToVector(app_socket, &routes, &data));
    std::string command(MessageToString(data[0]));
    if (command == kForward) {
      LOG(INFO) << "Inhere";
      CHECK_GE(data.size(), 3);
      ClientRequest* client_request(
          InterpretMessage<ClientRequest*>(*data[2]));
      ForwardRemote(MessageToString(data[1]),
                    client_request,
                    data.begin() + 3, data.end());
      // ForwardRemote handles its own message delete or delegates it.
      return;
    } else if (command == kCall) {
      CHECK_EQ(data[1]->size(), sizeof(Closure*));
      static_cast<Closure*>(data[1]->data())->Run();
      ReplyOK(app_socket, routes);
    } else if (command == kHello) {
      ReplyOK(app_socket, routes);
    } else if (command == kAddRemote) {
      CHECK_EQ(data.size(), 3);
      AddRemoteEndpoint(MessageToString(data[1]),
                        MessageToString(data[2]));
      ReplyOK(app_socket, routes);
      LOG(INFO)<<"Remote added.";
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
    client_request->closure->Run();
    DeleteContainerPointers(messages.begin(), messages.begin() + 2);
  }

  bool should_quit_;
  bool is_dirty_;
  zmq::context_t* context_;
  std::vector<zmq::socket_t*> sockets_;
  typedef std::map<std::string, zmq::socket_t*> ConnectionMap;
  typedef std::map<uint64, ClientRequest*> ClientRequestMap;
  ConnectionMap connection_map_;
  ClientRequestMap client_request_map_;
  EventIdGenerator event_id_generator_;
  DISALLOW_COPY_AND_ASSIGN(EventManagerThread);
};

namespace {
void EventManagerThreadEntryPoint(const
                                  EventManagerThreadParams params) {
  EventManagerThread emt(params.dealer_endpoint,
                         params.pubsub_endpoint,
                         params.context);
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
