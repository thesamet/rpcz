// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_EVENT_MANAGER_H
#define ZRPC_EVENT_MANAGER_H

#include <pthread.h>
#include <string>
#include <vector>

#include "zrpc/macros.h"

namespace zmq {
class context_t;
class message_t;
}  // namespace zmq

namespace zrpc {
struct ClientRequest;
class EventManagerController;
class RpcChannel;
template<class T>
class PointerVector;
typedef PointerVector<zmq::message_t> MessageVector;
class StoppingCondition;

class EventManager {
  public:
    EventManager(zmq::context_t* context, int nthreads = 1);

    explicit EventManager(int nthreads = 1);

    inline int GetThreadCount() const { return nthreads_; }

    ~EventManager();

  private:
    EventManagerController* GetController() const;

    void Init();

    zmq::context_t* context_;
    int nthreads_;
    std::vector<pthread_t> threads_;
    pthread_t worker_device_thread_;
    pthread_t pubsub_device_thread_;
    pthread_key_t controller_key_;
    bool owns_context_;
    friend class Connection;
    friend class ConnectionImpl;
    DISALLOW_COPY_AND_ASSIGN(EventManager);
};

void InstallSignalHandler();

class Connection {
 public:
  static Connection* CreateConnection(
      EventManager* em, const std::string& endpoint);

  virtual void SendClientRequest(ClientRequest* client_request,
                                 const MessageVector& messages) = 0;

  virtual int WaitUntil(StoppingCondition* stopping_condition) = 0;

  // Creates a thread-specific RpcChannel for this connection.
  virtual RpcChannel* MakeChannel() = 0;

 protected:
  Connection() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(Connection);
};
}  // namespace zrpc
#endif
