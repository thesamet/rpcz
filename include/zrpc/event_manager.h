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
}  // namespace zmq

namespace zrpc {
class EventManagerController;
class RpcChannel;

class EventManager {
  public:
    explicit EventManager(zmq::context_t* context,
                          int nthreads = 1);

    inline int GetThreadCount() const { return nthreads_; }

    ~EventManager();

  private:
    EventManagerController* GetController() const;

    zmq::context_t* context_;
    int nthreads_;
    std::vector<pthread_t> threads_;
    pthread_t worker_device_thread_;
    pthread_t pubsub_device_thread_;
    pthread_key_t controller_key_;
    friend class Connection;
    friend class ConnectionImpl;
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
