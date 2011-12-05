// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_EVENT_MANAGER_H
#define ZRPC_EVENT_MANAGER_H

#include <pthread.h>
#include <string>
#include <vector>

#include "macros.h"

namespace zmq {
class context_t;
class message_t;
class socket_t;
}  // namespace zmq

namespace zrpc {
class EventManagerController;
class RpcChannel;

struct ClientRequest {
  enum Status {
    OK = 0,
    FAILED = 1
  };
  Status status;
  std::vector<zmq::message_t*> return_path;
};

class EventManager {
  public:
    explicit EventManager(zmq::context_t* context,
                          int nthreads = 1);

    EventManagerController* GetController() const;

    inline int GetThreadCount() { return nthreads_; }

    ~EventManager();
  private:
    zmq::context_t* context_;
    int nthreads_;
    std::vector<pthread_t> threads_;
    pthread_t worker_device_thread_;
    pthread_t pubsub_device_thread_;
    DISALLOW_COPY_AND_ASSIGN(EventManager);
};

class Connection {
 public:
  static Connection* CreateConnection(
      EventManager* em, const std::string& endpoint);

  // Creates a thread-specific RpcChannel for this connection.
  virtual RpcChannel* MakeChannel() = 0;

 protected:
  Connection() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(Connection);
};
}  // namespace zrpc
#endif
