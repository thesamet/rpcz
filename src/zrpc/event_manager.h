// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_EVENT_MANAGER_H
#define ZRPC_EVENT_MANAGER_H

#include "macros.h"
#include <pthread.h>
#include <string>
#include <vector>

namespace zmq {
class context_t;
class message_t;
}  // namespace zmq

namespace google{
namespace protobuf {
class Closure;
}  // namespace protobuf
}  // namespace google

namespace zrpc {

struct ClientRequest {
  enum Status {
    OK = 0,
    FAILED = 1
  };
  Status status;
  std::vector<zmq::message_t*> result;
  google::protobuf::Closure* closure;
};

class EventManagerController;

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
    std::vector<pthread_t> threads;
    DISALLOW_COPY_AND_ASSIGN(EventManager);
};

class EventManagerController {
 public:
  virtual void AddRemoteEndpoint(const std::string& remote_name,
                                 const std::string& remote_endpoint) = 0;

  virtual void Quit() = 0;

  virtual ~EventManagerController() {};
};
}  // namespace

#endif
