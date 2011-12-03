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

using google::protobuf::uint64;;

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
}

namespace zrpc {

using google::protobuf::Closure;

namespace {
void StartEventManagerThread(zmq::context_t* context,
                             const char* app_endpoint,
                             const char* clients_endpoint,
                             pthread_t *thread);

const static char *kHello = "HELLO";
const static char *kAddRemote = "ADD_REMOTE";
const static char *kCall = "CALL";
const static char *kForward = "FORWARD";
const static char *kQuit = "QUIT";

class EventManagerControllerImpl : public EventManagerController {
 public:
  EventManagerControllerImpl(zmq::context_t* context,
                             const std::string& app_endpoint)
      : socket_(*context, ZMQ_REQ) {
    socket_.connect(app_endpoint.c_str());
  }

  void AddRemoteEndpoint(const std::string& remote_name,
                         const std::string& remote_endpoint) {
    zmq::message_t msg;
    SendString(&socket_, kAddRemote, ZMQ_SNDMORE);
    SendString(&socket_, remote_name, ZMQ_SNDMORE);
    SendString(&socket_, remote_endpoint, 0);
    CheckReply();
  }

  void Quit() {
    SendString(&socket_, kQuit, 0);
    CheckReply();
  }

 private:
  void CheckReply() {
    zmq::message_t msg;
    socket_.recv(&msg);
    CHECK_EQ(MessageToString(&msg), "OK");
  }

  zmq::socket_t socket_;
};
}  // unnamed namespace

EventManager::EventManager(zmq::context_t* context,
                           int nthreads) : context_(context),
                                           nthreads_(nthreads),
                                           threads(nthreads) {
  zmq::socket_t sync(*context, ZMQ_REP);
  sync.bind("inproc://clients.sync");
  for (int i = 0; i < nthreads; ++i) {
    StartEventManagerThread(context,
                            "inproc://clients.router",
                            "inproc://clients.hub",
                            &threads[i]);
  }
  zmq::message_t msg;
  sync.recv(&msg);
  CHECK(MessageToString(&msg) == kHello);
  VLOG(2) << "EventManager is up.";
}

EventManagerController* EventManager::GetController() const {
  return new EventManagerControllerImpl(context_,
                                        "inproc://clients.router");
}

EventManager::~EventManager() {
  EventManagerController *controller = GetController();
  controller->Quit();
  delete controller;
  VLOG(2) << "Waiting for EventManagerThreads to quit.";
  for (int i = 0; i < GetThreadCount(); ++i) {
    CHECK_EQ(pthread_join(threads[i], NULL), 0);
  }
  VLOG(2) << "EventManagerTheads finished.";
}

class EventManagerThread {
 public:
  explicit EventManagerThread(
      const std::string& app_endpoint,
      const std::string& clients_endpoint,
      zmq::context_t *context)
      : app_endpoint_(app_endpoint),
        clients_endpoint_(clients_endpoint),
        context_(context),
        should_quit_(false),
        is_dirty_(true) {
        }

  void Start() {
    app_socket_ = new zmq::socket_t(*context_, ZMQ_ROUTER);
    app_socket_->bind(app_endpoint_.c_str());
    app_socket_->setsockopt(ZMQ_IDENTITY, "appsocket", 9);
    clients_socket_ = new zmq::socket_t(*context_, ZMQ_ROUTER);
    clients_socket_->bind(clients_endpoint_.c_str());
    NotifyReady();
    sockets_.push_back(app_socket_);
    sockets_.push_back(clients_socket_);
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
        if (pollitems[i].socket == *app_socket_) {
          HandleAppSocket();
        } else if (pollitems[i].socket == *clients_socket_) {
          CHECK(false) << "Not sure what it is for yet.";
        } else {
          HandleClientSocket(sockets_[i]);
        }
      }
    };
  }

  ~EventManagerThread() {
    DeleteContainerPointers(sockets_.begin(),
                            sockets_.end());
  }

  std::string app_endpoint_;
  std::string clients_endpoint_;

 private:
  void RebuildPollItems(std::vector<zmq::pollitem_t>* pollitems) {
    pollitems->resize(sockets_.size());
    for (int i = 0; i < sockets_.size(); ++i) {
      zmq::socket_t& socket = *sockets_[i];
      zmq::pollitem_t pollitem = {socket, 0, ZMQ_POLLIN, 0};
      (*pollitems)[i] = pollitem;
    }
  }

  void NotifyReady() {
    zmq::socket_t sync(*context_, ZMQ_REQ);
    sync.connect("inproc://clients.sync");
    SendString(&sync, kHello);
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

  void ReplyOK(const std::vector<zmq::message_t*>& routes) {
    zmq::message_t* msg = StringToMessage("OK");
    WriteVectorsToSocket(app_socket_, routes, std::vector<zmq::message_t*>(
            1, msg));
    delete msg;
  }

  void HandleAppSocket() {
    std::vector<zmq::message_t*> routes;
    std::vector<zmq::message_t*> data;
    CHECK(ReadMessageToVector(app_socket_, &routes, &data));
    std::string command(MessageToString(data[0]));
    if (command == kForward) {
      CHECK_GE(data.size(), 3);
      ClientRequest* client_request(
          InterpretMessage<ClientRequest*>(*data[2]));
      ForwardRemote(MessageToString(data[1]),
                    client_request,
                    data.begin() + 3, data.end());
      // ForwardRemote handles its own message delete or delegates it.
      return;
    } else if (command == kCall) {
      CHECK_EQ(data[1]->size(), sizeof(google::protobuf::Closure*));
      static_cast<google::protobuf::Closure*>(data[1]->data())->Run();
      ReplyOK(routes);
    } else if (command == kQuit) {
      should_quit_ = true;
      ReplyOK(routes);
    } else if (command == kAddRemote) {
      CHECK_EQ(data.size(), 3);
      AddRemoteEndpoint(MessageToString(data[1]),
                        MessageToString(data[2]));
      ReplyOK(routes);
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
    std::copy(messages.begin() + 2, messages.end(), client_request->result.begin());
    client_request->closure->Run();
    DeleteContainerPointers(messages.begin(), messages.begin() + 2);
  }

  bool should_quit_;
  bool is_dirty_;
  zmq::socket_t* clients_socket_;
  zmq::socket_t* app_socket_;
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
struct EventManagerThreadParams {
  zmq::context_t* context;
  const char* app_endpoint;
  const char* clients_endpoint;
};

void* EventManagerThreadEntryPoint(void* args) {
  EventManagerThreadParams *params(
      static_cast<EventManagerThreadParams*>(args));
  EventManagerThread emt(params->app_endpoint, params->clients_endpoint,
                         params->context);
  emt.Start();
  delete params;
  return NULL;
}

void StartEventManagerThread(zmq::context_t* context,
                             const char* app_endpoint,
                             const char* clients_endpoint,
                             pthread_t* thread) {
  EventManagerThreadParams *params = new EventManagerThreadParams;
  params->context = context;
  params->app_endpoint = app_endpoint;
  params->clients_endpoint = clients_endpoint;
  CHECK(pthread_create(thread,
                       NULL,
                       EventManagerThreadEntryPoint,
                       params) == 0);
}
}  // unnamed namespace


}  // namespace zrpc
