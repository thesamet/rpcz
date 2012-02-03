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
#include "boost/lexical_cast.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/tss.hpp"
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
#include "rpcz/event_manager.h"
#include "rpcz/logging.h"
#include "rpcz/macros.h"
#include "rpcz/reactor.h"
#include "rpcz/remote_response.h"
#include "rpcz/sync_event.h"

namespace rpcz {
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
}  // unnamed namespace

struct RemoteResponseWrapper {
  RemoteResponse* remote_response;
  int64 deadline_ms;
  uint64 start_time;
  Closure* closure;
  MessageVector return_path;
};

class ConnectionImpl : public Connection {
 public:
  ConnectionImpl(ConnectionManager* connection_manager)
      : Connection(), connection_manager_(connection_manager) {}

  virtual ~ConnectionImpl() {}

  virtual void SendRequest(
      MessageVector* request,
      RemoteResponse* response,
      int64 deadline_ms,
      Closure* closure) {
    RemoteResponseWrapper* wrapper = new RemoteResponseWrapper;
    wrapper->remote_response = response;
    wrapper->start_time = zclock_time();
    wrapper->deadline_ms = deadline_ms;
    wrapper->closure = closure;

    zmq::socket_t& socket = connection_manager_->GetFrontendSocket();
    SendEmptyMessage(&socket, ZMQ_SNDMORE);
    SendString(&socket, "REQUEST", ZMQ_SNDMORE);
    SendPointer(&socket, this, ZMQ_SNDMORE);
    SendPointer(&socket, wrapper, ZMQ_SNDMORE);
    WriteVectorToSocket(&socket, *request);
  }

 private:
  ConnectionManager* connection_manager_;
  zmq::socket_t* socket_;
  friend class ConnectionManagerThread;
  DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

class ConnectionManagerThread {
 public:
  ConnectionManagerThread(
      zmq::context_t* context,
      EventManager* external_event_manager,
      ConnectionManager* connection_manager) {
    context_ = context;
    external_event_manager_ = external_event_manager;
    connection_manager_ = connection_manager;
  }

  static void Run(zmq::context_t* context,
                  EventManager* external_event_manager,
                  zmq::socket_t* frontend_socket,
                  ConnectionManager* connection_manager) {
    ConnectionManagerThread cmt(context, external_event_manager,
                                connection_manager);
    cmt.reactor_.AddSocket(frontend_socket, NewPermanentCallback(
            &cmt, &ConnectionManagerThread::HandleFrontendSocket,
            frontend_socket));
    cmt.reactor_.Loop();
  }

  void HandleFrontendSocket(zmq::socket_t* frontend_socket) {
    MessageIterator iter(*frontend_socket);
    std::string sender = MessageToString(iter.next());
    CHECK_EQ(0, iter.next().size());
    std::string command(MessageToString(iter.next()));
    if (command == "QUIT") {
      reactor_.SetShouldQuit();
      return;
    } else if (command == "CONNECT") {
      std::string endpoint(MessageToString(iter.next()));
      ConnectionImpl* conn = new ConnectionImpl(connection_manager_);
      conn->socket_ = new zmq::socket_t(*context_, ZMQ_DEALER);
      int linger_ms = 0;
      conn->socket_->setsockopt(ZMQ_LINGER, &linger_ms, sizeof(linger_ms));
      conn->socket_->connect(endpoint.c_str());
      reactor_.AddSocket(conn->socket_, NewPermanentCallback(
              this, &ConnectionManagerThread::HandleClientSocket,
              conn->socket_));
                                                             
      SendString(frontend_socket, sender, ZMQ_SNDMORE);
      SendEmptyMessage(frontend_socket, ZMQ_SNDMORE);
      SendPointer(frontend_socket, conn, 0);
    } else if (command == "REQUEST") {
      ConnectionImpl* conn = InterpretMessage<ConnectionImpl*>(iter.next());
      RemoteResponseWrapper* remote_response =
          InterpretMessage<RemoteResponseWrapper*>(iter.next());
      SendRequest(conn, iter, remote_response);
    }
  }

  void HandleClientSocket(zmq::socket_t* socket) {
    MessageVector messages;
    CHECK(ReadMessageToVector(socket, &messages));
    CHECK(messages.size() >= 1);
    CHECK(messages[0].size() == 0);
    EventId event_id(InterpretMessage<EventId>(messages[1]));
    RemoteResponseMap::iterator iter = remote_response_map_.find(event_id);
    if (iter == remote_response_map_.end()) {
      return;
    }
    RemoteResponseWrapper*& remote_response_wrapper = iter->second;
    RemoteResponse*& remote_response = remote_response_wrapper->remote_response;
    remote_response->reply.transfer(2, messages.size(), messages);
    remote_response->status = RemoteResponse::DONE;
    if (remote_response_wrapper->closure) {
      if (external_event_manager_) {
        external_event_manager_->Add(remote_response_wrapper->closure);
      } else {
        LOG(ERROR) << "Can't run closure: no event manager supplied.";
        delete remote_response_wrapper->closure;
      }
    }
    delete remote_response_wrapper;
    remote_response_map_.erase(iter);
  }

  void SendRequest(ConnectionImpl* connection,
                   MessageIterator& iter,
                   RemoteResponseWrapper* remote_response_wrapper) {
    EventId event_id = event_id_generator_.GetNext();
    remote_response_map_[event_id] = remote_response_wrapper;
    if (remote_response_wrapper->deadline_ms != -1) {
      reactor_.RunClosureAt(
          remote_response_wrapper->start_time +
              remote_response_wrapper->deadline_ms,
          NewCallback(this, &ConnectionManagerThread::HandleTimeout, event_id));
    }

    SendString(connection->socket_, "", ZMQ_SNDMORE);
    SendUint64(connection->socket_, event_id, ZMQ_SNDMORE);
    while (iter.has_more()) {
      connection->socket_->send(iter.next(), iter.has_more() ? ZMQ_SNDMORE : 0);
    }
  }

  void HandleTimeout(EventId event_id) {
    RemoteResponseMap::iterator iter = remote_response_map_.find(event_id);
    if (iter == remote_response_map_.end()) {
      return;
    }
    RemoteResponseWrapper*& remote_response_wrapper = iter->second;
    RemoteResponse*& remote_response = remote_response_wrapper->remote_response;
    remote_response->status = RemoteResponse::DEADLINE_EXCEEDED;
    if (remote_response_wrapper->closure) {
      if (external_event_manager_) {
        external_event_manager_->Add(remote_response_wrapper->closure);
      } else {
        LOG(ERROR) << "Can't run closure: no event manager supplied.";
        delete remote_response_wrapper->closure;
      }
    }
    delete remote_response_wrapper;
    remote_response_map_.erase(iter);
  }

 private:
  typedef std::map<EventId, RemoteResponseWrapper*> RemoteResponseMap;
  typedef std::map<uint64, EventId> DeadlineMap;
  ConnectionManager* connection_manager_;
  RemoteResponseMap remote_response_map_;
  DeadlineMap deadline_map_;
  EventIdGenerator event_id_generator_;
  EventManager* external_event_manager_;
  Reactor reactor_;
  zmq::context_t* context_;
};

ConnectionManager::ConnectionManager(
    zmq::context_t* context, EventManager* event_manager)
  : context_(context),
    external_event_manager_(event_manager),
    frontend_endpoint_("inproc://" +
                       boost::lexical_cast<std::string>(this) + ".cm.frontend")
{
  zmq::socket_t* frontend_socket = new zmq::socket_t(*context, ZMQ_ROUTER);
  frontend_socket->bind(frontend_endpoint_.c_str());
  thread_ = boost::thread(&ConnectionManagerThread::Run,
                          context, external_event_manager_,
                          frontend_socket, this);
}

zmq::socket_t& ConnectionManager::GetFrontendSocket() {
  zmq::socket_t* socket = socket_.get();
  if (socket == NULL) {
    LOG(INFO) << "Creating socket. Context_=" << (size_t)context_;
    socket = new zmq::socket_t(*context_, ZMQ_DEALER);
    socket->connect(frontend_endpoint_.c_str());
    socket_.reset(socket);
  }
  return *socket;
}

Connection* ConnectionManager::Connect(const std::string& endpoint) {
  zmq::socket_t& socket = GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendString(&socket, "CONNECT", ZMQ_SNDMORE);
  SendString(&socket, endpoint, 0);
  zmq::message_t msg;
  socket.recv(&msg);
  socket.recv(&msg);
  Connection* connection = InterpretMessage<Connection*>(msg);
  return connection;
}
 
ConnectionManager::~ConnectionManager() {
  LOG(INFO) << "Tearing down";
  zmq::socket_t& socket = GetFrontendSocket();
  SendEmptyMessage(&socket, ZMQ_SNDMORE);
  SendString(&socket, "QUIT", 0);
  thread_.join();
  socket_.reset(NULL);
}
}  // namespace rpcz
