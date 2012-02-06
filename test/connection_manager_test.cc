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

#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <glog/logging.h>
#include <stdio.h>
#include <zmq.hpp>
#include "gtest/gtest.h"
#include "rpcz/callback.h"
#include "rpcz/connection_manager.h"
#include "rpcz/macros.h"
#include "rpcz/remote_response.h"
#include "rpcz/sync_event.h"
#include "rpcz/zmq_utils.h"


namespace rpcz {

class ConnectionManagerTest : public ::testing::Test {
 public:
  ConnectionManagerTest() : context(1) {}

 protected:
  zmq::context_t context;
};

TEST_F(ConnectionManagerTest, TestStartsAndFinishes) {
  ConnectionManager cm(&context, 4);
}

void EchoServer(zmq::socket_t *socket) {
  bool should_quit = false;
  int messages = 0;
  while (!should_quit) {
    MessageVector v;
    GOOGLE_CHECK(ReadMessageToVector(socket, &v));
    ++messages;
    ASSERT_EQ(4, v.size());
    if (MessageToString(v[2]) == "hello") {
      ASSERT_EQ("there", MessageToString(v[3]).substr(0, 5));
    } else if (MessageToString(v[2]) == "QUIT") {
      should_quit = true;
    } else {
      GOOGLE_CHECK(false) << "Unknown command: " << MessageToString(v[2]);
    }
    WriteVectorToSocket(socket, v);
  }
  delete socket;
}

boost::thread StartServer(zmq::context_t* context) {
  zmq::socket_t* server = new zmq::socket_t(*context, ZMQ_DEALER);
  server->bind("inproc://server.test");
  return boost::thread(boost::bind(EchoServer, server));
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
  ASSERT_EQ(RemoteResponse::DONE, response->status);
  ASSERT_EQ(2, response->reply.size());
  ASSERT_EQ("hello", MessageToString(response->reply[0]));
  ASSERT_EQ("there_0", MessageToString(response->reply[1]));
  sync->Signal();
}

void ExpectTimeout(RemoteResponse* response, SyncEvent* sync) {
  ASSERT_EQ(RemoteResponse::DEADLINE_EXCEEDED, response->status);
  ASSERT_EQ(0, response->reply.size());
  sync->Signal();
}

TEST_F(ConnectionManagerTest, TestTimeoutAsync) {
  zmq::socket_t server(context, ZMQ_DEALER);
  server.bind("inproc://server.test");

  ConnectionManager cm(&context, 4);
  Connection connection(cm.Connect("inproc://server.test"));
  scoped_ptr<MessageVector> request(CreateSimpleRequest());
  RemoteResponse response;

  SyncEvent event;
  connection.SendRequest(*request, &response, 0,
                         NewCallback(ExpectTimeout, &response, &event));
  event.Wait();
  ASSERT_EQ(RemoteResponse::DEADLINE_EXCEEDED, response.status);
  ASSERT_EQ(0, response.reply.size());
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

void SendManyMessages(Connection connection, int thread_id) {
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
    connection.SendRequest(*request, response, -1,
                           &barrier);
  }
  barrier.Wait(request_count);
}

TEST_F(ConnectionManagerTest, ManyClientsTest) {
  boost::thread thread(StartServer(&context));
  ConnectionManager cm(&context, 4);

  Connection connection(cm.Connect("inproc://server.test"));
  boost::thread_group group;
  for (int i = 0; i < 10; ++i) {
    group.add_thread(
        new boost::thread(boost::bind(SendManyMessages, connection, i)));
  }
  group.join_all();
  scoped_ptr<MessageVector> request(CreateQuitRequest());
  RemoteResponse response;
  SyncEvent event;
  connection.SendRequest(*request, &response, -1,
                         NewCallback(&event, &SyncEvent::Signal));
  event.Wait();
  thread.join();
}

void HandleRequest(ClientConnection connection,
                   MessageIterator& request) {
  int value = boost::lexical_cast<int>(MessageToString(request.next()));
  MessageVector v;
  v.push_back(StringToMessage(boost::lexical_cast<std::string>(value + 1)));
  connection.Reply(&v);
}

TEST_F(ConnectionManagerTest, TestBindServer) {
  ConnectionManager cm(&context, 4);
  cm.Bind("inproc://server.point", &HandleRequest);
  Connection c = cm.Connect("inproc://server.point");
  MessageVector v;
  v.push_back(StringToMessage("317"));
  RemoteResponse response;
  SyncEvent event;
  c.SendRequest(v, &response, -1,
                NewCallback(&event, &SyncEvent::Signal));
  event.Wait();
  CHECK_EQ(1, response.reply.size());
  CHECK_EQ("318", MessageToString(response.reply[0]));
}

const static char* kEndpoint = "inproc://test";
const static char* kReply = "gotit";

void DoThis(zmq::context_t* context) {
  LOG(INFO)<<"Creating socket. Context="<<context;
  zmq::socket_t socket(*context, ZMQ_PUSH);
  socket.connect(kEndpoint);
  SendString(&socket, kReply);
  socket.close();
  LOG(INFO)<<"socket closed";
}

TEST_F(ConnectionManagerTest, ProcessesSingleCallback) {
  ConnectionManager cm(&context, 4);
  zmq::socket_t socket(context, ZMQ_PULL);
  socket.bind(kEndpoint);
  cm.Add(NewCallback(&DoThis, &context));
  MessageVector messages;
  CHECK(ReadMessageToVector(&socket, &messages));
  ASSERT_EQ(1, messages.size());
  CHECK_EQ(kReply, MessageToString(messages[0]));
}

void Increment(boost::mutex* mu,
               boost::condition_variable* cond, int* x) {
  mu->lock();
  (*x)++;
  cond->notify_one();
  mu->unlock();
}

void AddManyClosures(ConnectionManager* cm) {
  boost::mutex mu;
  boost::condition_variable cond;
  boost::unique_lock<boost::mutex> lock(mu);
  int x = 0;
  const int kMany = 137;
  for (int i = 0; i < kMany; ++i) {
    cm->Add(NewCallback(&Increment, &mu, &cond, &x));
  }
  CHECK_EQ(0, x);  // since we are holding the lock
  while (x != kMany) {
    cond.wait(lock);
  }
}

TEST_F(ConnectionManagerTest, ProcessesManyCallbacksFromManyThreads) {
  const int thread_count = 10;
  ConnectionManager cm(&context, thread_count);
  boost::thread_group thread_group;
  for (int i = 0; i < thread_count; ++i) {
    thread_group.add_thread(
        new boost::thread(boost::bind(AddManyClosures, &cm)));
  }
  thread_group.join_all();
}
}  // namespace rpcz
