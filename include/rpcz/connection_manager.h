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

#ifndef RPCZ_CONNECTION_MANAGER_H
#define RPCZ_CONNECTION_MANAGER_H

#include <string>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include "rpcz/macros.h"
#include "rpcz/sync_event.h"

namespace zmq {
class context_t;
class message_t;
class socket_t;
}  // namespace zmq

namespace rpcz {
class ClientConnection;
class Closure;
class Connection;
class ConnectionThreadContext;
class MessageIterator;
class MessageVector;
class RemoteResponse;

namespace internal {
struct ThreadContext;
}  // namespace internal

// A ConnectionManager is a multi-threaded asynchronous system for communication
// over ZeroMQ sockets. A ConnectionManager can:
//   1. Connect to a remote server and allow all threads the program to share
//      the connection:
//
//          ConnectionManager cm(10);
//          Connection c = cm.Connect("tcp://localhost:5557");
// 
//      Now, it is possible to send requests to this backend fron any thread:
//
//          c.SendRequest(...);
//
//  2. Bind a socket and register a handler function. The handler function
//     gets executed in one of the connection manager threads.
//
//          c.Bind("tcp://*:5555", &HandlerFunction);
//
//  3. Queue closures to be executed on one of the connection manager threads:
//
//          c.Add(closure)
//
// ConnectionManager and Connection are thread-safe.
class ConnectionManager {
 public:
  enum Status {
    INACTIVE = 0,
    ACTIVE = 1,
    DONE = 2,
    DEADLINE_EXCEEDED = 3,
  };

  typedef boost::function<void(const ClientConnection&, MessageIterator&)>
      ServerFunction;
  typedef boost::function<void(Status status, MessageIterator&)>
      ClientRequestCallback;

  // Constructs a ConnectionManager that has nthreads worker threads. The
  // ConnectionManager does not take ownership of the given ZeroMQ context.
  ConnectionManager(zmq::context_t* context, int nthreads);

  // Blocks the current thread until all connection managers have completed.
  virtual ~ConnectionManager();

  // Connects all ConnectionManager threads to the given endpoint. On success
  // this method returns a Connection object that can be used from any thread
  // to communicate with this endpoint.
  virtual Connection Connect(const std::string& endpoint);

  // Binds a socket to the given endpoint and registers ServerFunction as a
  // handler for requests to this socket. The function gets executed on one of
  // the worker threads. When the function returns, the endpoint is already
  // bound.
  virtual void Bind(const std::string& endpoint, ServerFunction function);

  // Executes the closure on one of the worker threads.
  virtual void Add(Closure* closure);

  // Blocks this thread until Terminate() is called from another thread.
  virtual void Run();

  // Releases all the threads that are blocked inside Run()
  virtual void Terminate();

 private:
  zmq::context_t* context_;

  inline zmq::socket_t& GetFrontendSocket();

  boost::thread broker_thread_;
  boost::thread_group worker_threads_;
  boost::thread_specific_ptr<zmq::socket_t> socket_;
  std::string frontend_endpoint_;
  SyncEvent is_termating_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
  friend class Connection;
  friend class ClientConnection;
  friend class ConnectionManagerThread;
};

// Installs a SIGINT and SIGTERM handlers that causes all RPCZ's event loops
// to cleanly quit.
void InstallSignalHandler();

// Represents a connection to a server. Thread-safe.
class Connection {
 public:
  Connection() : manager_((ConnectionManager*)0xbadecafe), connection_id_(0) {}

  // Asynchronously sends a request over the connection.
  // request: a vector of messages to be sent. Does not take ownership of the
  //          request. The vector has to live valid at least until the request
  //          completes. It can be safely de-allocated inside the provided
  //          closure or after remote_response->Wait() returns.
  // deadline_ms - milliseconds before giving up on this request. -1 means
  //               forever.
  // callback - a closure that will be ran on one of the worker threads when a
  //           response arrives or it timeouts.
  void SendRequest(
      MessageVector& request,
      int64 deadline_ms,
      ConnectionManager::ClientRequestCallback callback);

 private:
  Connection(ConnectionManager *manager, uint64 connection_id) :
      manager_(manager), connection_id_(connection_id) {}
  ConnectionManager* manager_;
  uint64 connection_id_;
  friend class ConnectionManager;
};

class ClientConnection {
 public:
  void Reply(MessageVector* v);

 private:
  ClientConnection(ConnectionManager* manager, uint64 socket_id,
                   std::string& sender, std::string& event_id)
      : manager_(manager), socket_id_(socket_id), sender_(sender),
      event_id_(event_id) {}

  ConnectionManager* manager_;
  uint64 socket_id_;
  const std::string sender_;
  const std::string event_id_;
  friend void WorkerThread(ConnectionManager*, zmq::context_t*, std::string);
};
}  // namespace rpcz
#endif
