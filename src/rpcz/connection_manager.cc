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

#include "rpcz/connection_manager.h"

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/tss.hpp>
#include <map>
#include <ostream>
#include <pthread.h>
#include <sstream>
#include <stddef.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>
#include "zmq.h"
#include "zmq.hpp"

#include "google/protobuf/stubs/common.h"
#include "rpcz/callback.h"
#include "rpcz/clock.h"
#include "rpcz/logging.h"
#include "rpcz/macros.h"
#include "rpcz/reactor.h"
#include "rpcz/zmq_utils.h"

namespace rpcz {
namespace {
const uint64 kLargePrime = (1ULL << 63) - 165;
const uint64 kGenerator = 2;

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

// Command codes for internal process communication.
//
// Message sent from outside to the broker thread:
const char kRequest = 0x01;      // Send request to a connected socket.
const char kConnect = 0x02;      // Connect to a given endpoint.
const char kBind    = 0x03;      // Bind to an endpoint.
const char kReply   = 0x04;      // Reply to a request
const char kQuit    = 0x0f;      // Starts the quit second.

// Messages sent from the broker to a worker thread:
const char kRunClosure        = 0x11;   // Run a closure
const char kRunServerFunction = 0x12;   // Handle a request (a reply path
                                        // is given)
const char kInvokeClientRequestCallback = 0x13;  // Run a user supplied
                                                 // function that processes
                                                 // a reply from a remote
                                                 // server.
const char kWorkerQuit = 0x1f;          // Asks the worker to quit.

// Messages sent from a worker thread to the broker:
const char kReady = 0x21;        // Always the first message sent.
const char kWorkerDone = 0x22;   // Sent just before the worker quits.
}  // unnamed namespace

struct RemoteResponseWrapper {
  int64 deadline_ms;
  uint64 start_time;
  ConnectionManager::ClientRequestCallback callback;
};

void Connection::SendRequest(
    MessageVector& request,
    int64 deadline_ms,
    ConnectionManager::ClientRequestCallback callback) {
  RemoteResponseWrapper wrapper;
  wrapper.start_time = zclock_time();
  wrapper.deadline_ms = deadline_ms;
  wrapper.callback = callback;

  zmq::socket_t& socket = manager_->GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kRequest, ZMQ_SNDMORE);
  SendUint64(&socket, connection_id_, ZMQ_SNDMORE);
  SendObject(&socket, wrapper, ZMQ_SNDMORE);
  WriteVectorToSocket(&socket, request);
}

void ClientConnection::Reply(MessageVector* v) {
  zmq::socket_t& socket = manager_->GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kReply, ZMQ_SNDMORE);
  SendUint64(&socket, socket_id_, ZMQ_SNDMORE);
  SendString(&socket, sender_, ZMQ_SNDMORE);
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendString(&socket, event_id_, ZMQ_SNDMORE);
  WriteVectorToSocket(&socket, *v);
}

void WorkerThread(ConnectionManager* connection_manager,
                  zmq::context_t* context, std::string endpoint) {
  zmq::socket_t socket(*context, ZMQ_DEALER);
  socket.connect(endpoint.c_str());
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kReady);
  bool should_quit = false;
  while (!should_quit) {
    MessageIterator iter(socket);
    CHECK_EQ(0, iter.next().size());
    char command(InterpretMessage<char>(iter.next()));
    switch (command) {
      case kWorkerQuit:
        should_quit = true;
        break;
      case kRunClosure:
        InterpretMessage<Closure*>(iter.next())->Run();
        break;
      case kRunServerFunction: {
        ConnectionManager::ServerFunction sf =
            InterpretMessage<ConnectionManager::ServerFunction>(iter.next());
        uint64 socket_id = InterpretMessage<uint64>(iter.next());
        std::string sender(MessageToString(iter.next()));
        if (iter.next().size() != 0) {
          break;
        }
        std::string event_id(MessageToString(iter.next()));
        sf(ClientConnection(connection_manager, socket_id, sender, event_id),
           iter);
        }
        break;
      case kInvokeClientRequestCallback: {
        ConnectionManager::ClientRequestCallback cb =
            InterpretMessage<ConnectionManager::ClientRequestCallback>(
                iter.next());
        ConnectionManager::Status status = ConnectionManager::Status(
            InterpretMessage<uint64>(iter.next()));
        cb(status, iter);
      }
    }
  }
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kWorkerDone);
}

class ConnectionManagerThread {
 public:
  ConnectionManagerThread(
      zmq::context_t* context,
      int nthreads,
      SyncEvent* ready_event,
      ConnectionManager* connection_manager,
      zmq::socket_t* frontend_socket) : 
    connection_manager_(connection_manager),
    context_(context),
    frontend_socket_(frontend_socket),
    current_worker_(0) {
      WaitForWorkersReadyReply(nthreads);
      ready_event->Signal();
      reactor_.AddSocket(
          frontend_socket, NewPermanentCallback(
              this, &ConnectionManagerThread::HandleFrontendSocket,
              frontend_socket));
    }

  void WaitForWorkersReadyReply(int nthreads) {
    for (int i = 0; i < nthreads; ++i) {
      MessageIterator iter(*frontend_socket_);
      std::string sender = MessageToString(iter.next());
      CHECK_EQ(0, iter.next().size());
      char command(InterpretMessage<char>(iter.next()));
      CHECK_EQ(kReady, command) << "Got unexpected command " << (int)command;
      workers_.push_back(sender);
    }
  }

  static void Run(zmq::context_t* context,
                  int nthreads,
                  SyncEvent* ready_event,
                  zmq::socket_t* frontend_socket,
                  ConnectionManager* connection_manager) {
    ConnectionManagerThread cmt(context, nthreads,
                                ready_event,
                                connection_manager, frontend_socket);
    cmt.reactor_.Loop();
  }

  void HandleFrontendSocket(zmq::socket_t* frontend_socket) {
    MessageIterator iter(*frontend_socket);
    std::string sender = MessageToString(iter.next());
    CHECK_EQ(0, iter.next().size());
    char command(InterpretMessage<char>(iter.next()));
    switch (command) {
      case kQuit:
        // Ask the workers to quit. They'll in turn send kWorkerDone.
        for (int i = 0; i < workers_.size(); ++i) {
          SendString(frontend_socket_, workers_[i], ZMQ_SNDMORE);
          SendEmptyMessage(frontend_socket_, ZMQ_SNDMORE);
          SendChar(frontend_socket_, kWorkerQuit, 0);
        }
        break;
      case kConnect: 
        HandleConnectCommand(sender, MessageToString(iter.next()));
        break;
      case kBind: 
        HandleBindCommand(sender, MessageToString(iter.next()),
                          InterpretMessage<ConnectionManager::ServerFunction>(
                              iter.next()));
        break;
      case kRequest:
        SendRequest(iter);
        break;
      case kReply:
        SendReply(iter);
        break;
      case kReady:
        CHECK(false);
        break;
      case kWorkerDone:
        workers_.erase(std::remove(workers_.begin(), workers_.end(), sender));
        current_worker_ = 0;
        if (workers_.size() == 0) {
          // All workers are gone, time to quit.
          reactor_.SetShouldQuit();
        }
        break;
      case kRunClosure:
        AddClosure(InterpretMessage<Closure*>(iter.next()));
        break;
    }
  }

  inline void BeginWorkerCommand(char command) {
    SendString(frontend_socket_, workers_[current_worker_], ZMQ_SNDMORE);
    SendEmptyMessage(frontend_socket_, ZMQ_SNDMORE);
    SendChar(frontend_socket_, command, ZMQ_SNDMORE);
    ++current_worker_;
    if (current_worker_ == workers_.size()) {
      current_worker_ = 0;
    }
  }

  inline void AddClosure(Closure* closure) {
    BeginWorkerCommand(kRunClosure);
    SendPointer(frontend_socket_, closure, 0);
  }

  inline void HandleConnectCommand(const std::string& sender,
                                   const std::string& endpoint) {
    zmq::socket_t* socket = new zmq::socket_t(*context_, ZMQ_DEALER);
    connections_.push_back(socket);
    int linger_ms = 0;
    socket->setsockopt(ZMQ_LINGER, &linger_ms, sizeof(linger_ms));
    socket->connect(endpoint.c_str());
    reactor_.AddSocket(socket, NewPermanentCallback(
            this, &ConnectionManagerThread::HandleClientSocket,
            socket));

    SendString(frontend_socket_, sender, ZMQ_SNDMORE);
    SendEmptyMessage(frontend_socket_, ZMQ_SNDMORE);
    SendUint64(frontend_socket_, connections_.size() - 1, 0);
  }

  inline void HandleBindCommand(
      const std::string& sender,
      const std::string& endpoint,
      ConnectionManager::ServerFunction server_function) {
    zmq::socket_t* socket = new zmq::socket_t(*context_, ZMQ_ROUTER);
    int linger_ms = 0;
    socket->setsockopt(ZMQ_LINGER, &linger_ms, sizeof(linger_ms));
    socket->bind(endpoint.c_str());
    uint64 socket_id = server_sockets_.size();
    server_sockets_.push_back(socket);
    reactor_.AddSocket(socket, NewPermanentCallback(
            this, &ConnectionManagerThread::HandleServerSocket,
            socket_id, server_function));

    SendString(frontend_socket_, sender, ZMQ_SNDMORE);
    SendEmptyMessage(frontend_socket_, ZMQ_SNDMORE);
    SendEmptyMessage(frontend_socket_, 0);
  }

  void HandleServerSocket(uint64 socket_id,
                          ConnectionManager::ServerFunction server_function) {
    MessageIterator iter(*server_sockets_[socket_id]);
    BeginWorkerCommand(kRunServerFunction);
    SendObject(frontend_socket_, server_function, ZMQ_SNDMORE);
    SendUint64(frontend_socket_, socket_id, ZMQ_SNDMORE);
    ForwardMessages(iter, *frontend_socket_);
  }

  inline void SendRequest(MessageIterator& iter) {
    uint64 connection_id = InterpretMessage<uint64>(iter.next());
    RemoteResponseWrapper remote_response_wrapper =
        InterpretMessage<RemoteResponseWrapper>(iter.next());
    EventId event_id = event_id_generator_.GetNext();
    remote_response_map_[event_id] = remote_response_wrapper.callback;
    if (remote_response_wrapper.deadline_ms != -1) {
      reactor_.RunClosureAt(
          remote_response_wrapper.start_time +
              remote_response_wrapper.deadline_ms,
          NewCallback(this, &ConnectionManagerThread::HandleTimeout, event_id));
    }
    zmq::socket_t*& socket = connections_[connection_id];
    SendString(socket, "", ZMQ_SNDMORE);
    SendUint64(socket, event_id, ZMQ_SNDMORE);
    ForwardMessages(iter, *socket);
  }

  void HandleClientSocket(zmq::socket_t* socket) {
    MessageIterator iter(*socket);
    if (!iter.next().size() == 0) {
      return;
    }
    if (!iter.has_more()) {
      return;
    }
    EventId event_id(InterpretMessage<EventId>(iter.next()));
    RemoteResponseMap::iterator response_iter = remote_response_map_.find(event_id);
    if (response_iter == remote_response_map_.end()) {
      return;
    }
    ConnectionManager::ClientRequestCallback& callback = response_iter->second;
    BeginWorkerCommand(kInvokeClientRequestCallback);
    SendObject(frontend_socket_, callback, ZMQ_SNDMORE);
    SendUint64(frontend_socket_, ConnectionManager::DONE, ZMQ_SNDMORE);
    ForwardMessages(iter, *frontend_socket_);
    remote_response_map_.erase(response_iter);
  }

  void HandleTimeout(EventId event_id) {
    RemoteResponseMap::iterator response_iter = remote_response_map_.find(event_id);
    if (response_iter == remote_response_map_.end()) {
      return;
    }
    ConnectionManager::ClientRequestCallback& callback = response_iter->second;
    BeginWorkerCommand(kInvokeClientRequestCallback);
    SendObject(frontend_socket_, callback, ZMQ_SNDMORE);
    SendUint64(frontend_socket_, ConnectionManager::DEADLINE_EXCEEDED, 0);
    remote_response_map_.erase(response_iter);
  }

  inline void SendReply(MessageIterator& iter) {
    uint64 socket_id = InterpretMessage<uint64>(iter.next());
    zmq::socket_t* socket = server_sockets_[socket_id];
    ForwardMessages(iter, *socket);
  }

 private:
  typedef std::map<EventId, ConnectionManager::ClientRequestCallback>
      RemoteResponseMap;
  typedef std::map<uint64, EventId> DeadlineMap;
  ConnectionManager* connection_manager_;
  RemoteResponseMap remote_response_map_;
  DeadlineMap deadline_map_;
  EventIdGenerator event_id_generator_;
  Reactor reactor_;
  std::vector<zmq::socket_t*> connections_;
  std::vector<zmq::socket_t*> server_sockets_;
  zmq::context_t* context_;
  zmq::socket_t* frontend_socket_;
  std::vector<std::string> workers_;
  int current_worker_;
};

ConnectionManager::ConnectionManager(zmq::context_t* context, int nthreads)
  : context_(context),
    frontend_endpoint_(
        "inproc://" + boost::lexical_cast<std::string>(this) + ".cm.frontend") {
  zmq::socket_t* frontend_socket = new zmq::socket_t(*context, ZMQ_ROUTER);
  int linger_ms = 0;
  frontend_socket->setsockopt(ZMQ_LINGER, &linger_ms, sizeof(linger_ms));
  frontend_socket->bind(frontend_endpoint_.c_str());
  for (int i = 0; i < nthreads; ++i) {
    worker_threads_.add_thread(
        new boost::thread(&WorkerThread, this, context, frontend_endpoint_));
  }
  SyncEvent event;
  broker_thread_ = boost::thread(&ConnectionManagerThread::Run,
                                 context, nthreads, &event,
                                 frontend_socket, this);
  event.Wait();
}

zmq::socket_t& ConnectionManager::GetFrontendSocket() {
  zmq::socket_t* socket = socket_.get();
  if (socket == NULL) {
    socket = new zmq::socket_t(*context_, ZMQ_DEALER);
    int linger_ms = 0;
    socket->setsockopt(ZMQ_LINGER, &linger_ms, sizeof(linger_ms));
    socket->connect(frontend_endpoint_.c_str());
    socket_.reset(socket);
  }
  return *socket;
}

Connection ConnectionManager::Connect(const std::string& endpoint) {
  zmq::socket_t& socket = GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kConnect, ZMQ_SNDMORE);
  SendString(&socket, endpoint, 0);
  zmq::message_t msg;
  socket.recv(&msg);
  socket.recv(&msg);
  uint64 connection_id = InterpretMessage<uint64>(msg);
  return Connection(this, connection_id);
}

void ConnectionManager::Bind(const std::string& endpoint,
                             ServerFunction function) {
  zmq::socket_t& socket = GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kBind, ZMQ_SNDMORE);
  SendString(&socket, endpoint, ZMQ_SNDMORE);
  SendObject(&socket, function, 0);
  zmq::message_t msg;
  socket.recv(&msg);
  socket.recv(&msg);
  return;
}

void ConnectionManager::Add(Closure* closure) {
  zmq::socket_t& socket = GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kRunClosure, ZMQ_SNDMORE);
  SendPointer(&socket, closure, 0);
  return;
}
 
void ConnectionManager::Run() {
  is_termating_.Wait();
}

void ConnectionManager::Terminate() {
  is_termating_.Signal();
}

ConnectionManager::~ConnectionManager() {
  zmq::socket_t& socket = GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendChar(&socket, kQuit, 0);
  broker_thread_.join();
  worker_threads_.join_all();
  socket_.reset(NULL);
}
}  // namespace rpcz
