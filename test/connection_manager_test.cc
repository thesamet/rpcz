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

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <zmq.hpp>
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "zrpc/callback.h"
#include "zrpc/connection_manager.h"
#include "zrpc/event_manager.h"
#include "zrpc/macros.h"
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

void EchoServer(zmq::socket_t *socket, bool looping) {
  bool should_quit = false;
  int messages = 0;
  while (!should_quit && (looping || messages == 0)) {
    MessageVector v;
    CHECK(ReadMessageToVector(socket, &v));
    ++messages;
    CHECK_EQ(4, v.size());
    if (MessageToString(v[2]) == "hello") {
      CHECK_EQ("there", MessageToString(v[3]));
    } else if (MessageToString(v[2]) == "QUIT") {
      should_quit = true;
    } else {
      CHECK(false) << "Unknown command: " << MessageToString(v[2]);
    }
    WriteVectorToSocket(socket, v);
  }
  LOG(INFO) << "Quitting after " << messages << " messages.";
  delete socket;
}

boost::thread* StartServer(zmq::context_t* context, bool looping) {
  zmq::socket_t* server = new zmq::socket_t(*context, ZMQ_DEALER);
  server->bind("inproc://server.test");
  return CreateThread(NewCallback(&EchoServer, server, looping));
}

TEST_F(ConnectionManagerTest, TestSendRequestSync) {
  scoped_ptr<boost::thread> thread(StartServer(&context, false));

  ConnectionManager cm(&context, NULL, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  MessageVector request;
  request.push_back(StringToMessage("hello"));
  request.push_back(StringToMessage("there"));
  RemoteResponse response;

  connection->SendRequest(&request, &response, -1, NULL);
  response.Wait();
  CHECK_EQ(RemoteResponse::DONE, response.status);
  CHECK_EQ(2, response.reply.size());
  CHECK_EQ("hello", MessageToString(response.reply[0]));
  CHECK_EQ("there", MessageToString(response.reply[1]));
  thread->join();
}

void CheckResponse(RemoteResponse* response, SyncEvent* sync) {
  CHECK_EQ(RemoteResponse::DONE, response->status);
  CHECK_EQ(2, response->reply.size());
  CHECK_EQ("hello", MessageToString(response->reply[0]));
  CHECK_EQ("there", MessageToString(response->reply[1]));
  sync->Signal();
}

MessageVector* CreateSimpleRequest() {
  MessageVector* request = new MessageVector;
  request->push_back(StringToMessage("hello"));
  request->push_back(StringToMessage("there"));
  return request;
}

MessageVector* CreateQuitRequest() {
  MessageVector* request = new MessageVector;
  request->push_back(StringToMessage("QUIT"));
  request->push_back(StringToMessage(""));
  return request;
}

TEST_F(ConnectionManagerTest, TestSendRequestClosure) {
  scoped_ptr<boost::thread> thread(StartServer(&context, false));

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  scoped_ptr<MessageVector> request(CreateSimpleRequest());
  RemoteResponse response;

  SyncEvent event;
  connection->SendRequest(request.get(), &response, -1,
                          NewCallback(CheckResponse, &response, &event));
  event.Wait();
  thread->join();
  // Double-check:
  CHECK_EQ(RemoteResponse::DONE, response.status);
  CHECK_EQ(2, response.reply.size());
}

void ExpectTimeout(RemoteResponse* response, SyncEvent* sync) {
  CHECK_EQ(RemoteResponse::DEADLINE_EXCEEDED, response->status);
  CHECK_EQ(0, response->reply.size());
  sync->Signal();
}

TEST_F(ConnectionManagerTest, TestTimeoutSync) {
  zmq::socket_t server(context, ZMQ_DEALER);
  server.bind("inproc://server.test");

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  scoped_ptr<MessageVector> request(CreateSimpleRequest());
  RemoteResponse response;

  connection->SendRequest(request.get(), &response, 1, NULL);
  response.Wait();
  CHECK_EQ(RemoteResponse::DEADLINE_EXCEEDED, response.status);
  CHECK_EQ(0, response.reply.size());
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

void SendManyMessages(Connection* connection, bool sync_with_wait) {
  PointerVector<RemoteResponse> responses;
  PointerVector<MessageVector> requests;
  const int request_count = 100;
  BarrierClosure barrier;
  for (int i = 0; i < request_count; ++i) {
    MessageVector* request = CreateSimpleRequest();
    RemoteResponse* response = new RemoteResponse;
    responses.push_back(response);
    connection->SendRequest(request, response, -1, 
                            sync_with_wait ? NULL :
                                             &barrier);
  }
  if (sync_with_wait) {
    for (int i = 0; i < request_count; ++i) {
      responses[i]->Wait();
    }
  } else {
    barrier.Wait(request_count);
  }
}

TEST_F(ConnectionManagerTest, ManyClientsTest) {
  scoped_ptr<boost::thread> thread(StartServer(&context, true));

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 1);

  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  boost::thread_group group;
  for (int i = 0; i < 10; ++i) {
    group.add_thread(
        CreateThread(NewCallback(SendManyMessages, connection.get(), i < 5)));
  }
  group.join_all();
  scoped_ptr<MessageVector> request(CreateQuitRequest());
  RemoteResponse response;
  connection->SendRequest(request.get(), &response, -1, NULL);
  response.Wait();
  thread->join();
}
}  // namespace zrpc

int main(int argc, char** argv) {
  ::google::InstallFailureSignalHandler();
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = true;
  return RUN_ALL_TESTS();
}
