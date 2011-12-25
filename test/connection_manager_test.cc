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

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <stdio.h>
#include <zmq.hpp>
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "zrpc/callback.h"
#include "zrpc/connection_manager.h"
#include "zrpc/event_manager.h"
#include "zrpc/macros.h"
#include "zrpc/remote_response.h"
#include "zrpc/sync_event.h"
#include "zrpc/zmq_utils.h"


namespace zrpc {

class ConnectionManagerTest : public ::testing::Test {
 public:
  ConnectionManagerTest() : context(1) {}

 protected:
  zmq::context_t context;
};

TEST_F(ConnectionManagerTest, TestStartsAndFinishes) {
  ConnectionManager cm(&context, NULL, 2);
}

void EchoServer(zmq::socket_t *socket) {
  bool should_quit = false;
  int messages = 0;
  while (!should_quit) {
    MessageVector v;
    CHECK(ReadMessageToVector(socket, &v));
    ++messages;
    CHECK_EQ(4, v.size());
    if (MessageToString(v[2]) == "hello") {
      CHECK_EQ("there", MessageToString(v[3]).substr(0, 5));
    } else if (MessageToString(v[2]) == "QUIT") {
      should_quit = true;
    } else {
      CHECK(false) << "Unknown command: " << MessageToString(v[2]);
    }
    WriteVectorToSocket(socket, v);
  }
  delete socket;
  LOG(INFO) << "Quitting after " << messages << " messages.";

}

boost::thread* StartServer(zmq::context_t* context) {
  zmq::socket_t* server = new zmq::socket_t(*context, ZMQ_DEALER);
  server->bind("inproc://server.test");
  return new boost::thread(boost::bind(EchoServer, server));
}

MessageVector* CreateSimpleRequest(int number=0) {
  MessageVector* request = new MessageVector;
  request->push_back(StringToMessage("hello"));
  char str[256];
  sprintf(str, "there_%d", number);
  request->push_back(StringToMessage(str));
  return request;
}

MessageVector* CreateQuitRequest() {
  MessageVector* request = new MessageVector;
  request->push_back(StringToMessage("QUIT"));
  request->push_back(StringToMessage(""));
  return request;
}

void CheckResponse(RemoteResponse* response, SyncEvent* sync) {
  CHECK_EQ(RemoteResponse::DONE, response->status);
  CHECK_EQ(2, response->reply.size());
  CHECK_EQ("hello", MessageToString(response->reply[0]));
  CHECK_EQ("there_0", MessageToString(response->reply[1]));
  sync->Signal();
}

void ExpectTimeout(RemoteResponse* response, SyncEvent* sync) {
  CHECK_EQ(RemoteResponse::DEADLINE_EXCEEDED, response->status);
  CHECK_EQ(0, response->reply.size());
  sync->Signal();
}

TEST_F(ConnectionManagerTest, TestTimeoutAsync) {
  zmq::socket_t server(context, ZMQ_DEALER);
  server.bind("inproc://server.test");

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  scoped_ptr<MessageVector> request(CreateSimpleRequest());
  RemoteResponse response;

  SyncEvent event;
  connection->SendRequest(request.get(), &response, 0,
                          NewCallback(ExpectTimeout, &response, &event));
  event.Wait();
  CHECK_EQ(RemoteResponse::DEADLINE_EXCEEDED, response.status);
  CHECK_EQ(0, response.reply.size());
}

class BarrierClosure : public Closure {
 public:
  BarrierClosure() : count_(0) {}

  virtual void Run() {
    boost::unique_lock<boost::mutex> lock(mutex_);
    ++count_;
    cond_.notify_all();
  }

  virtual void Wait(int n) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    while (count_ < n) {
      cond_.wait(lock);
    }
  }

 private:
  boost::mutex mutex_;
  boost::condition_variable cond_;
  int count_;
};

void SendManyMessages(Connection* connection, int thread_id) {
  boost::ptr_vector<RemoteResponse> responses;
  boost::ptr_vector<MessageVector> requests;
  const int request_count = 100;
  BarrierClosure barrier;
  for (int i = 0; i < request_count; ++i) {
    MessageVector* request = CreateSimpleRequest(
        thread_id * request_count * 17 + i);
    requests.push_back(request);
    RemoteResponse* response = new RemoteResponse;
    responses.push_back(response);
    connection->SendRequest(request, response, -1,
                            &barrier);
  }
  barrier.Wait(request_count);
}

TEST_F(ConnectionManagerTest, ManyClientsTest) {
  scoped_ptr<boost::thread> thread(StartServer(&context));

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 1);

  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  boost::thread_group group;
  for (int i = 0; i < 10; ++i) {
    group.add_thread(
        new boost::thread(boost::bind(SendManyMessages, connection.get(), i)));
  }
  group.join_all();
  scoped_ptr<MessageVector> request(CreateQuitRequest());
  RemoteResponse response;
  connection->SendRequest(request.get(), &response, -1, NULL);
  // response.Wait();
  thread->join();
}
}  // namespace zrpc
