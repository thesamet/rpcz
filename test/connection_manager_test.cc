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
  ConnectionManagerTest() {}
 protected:
};

TEST_F(ConnectionManagerTest, TestStartsAndFinishes) {
  zmq::context_t context(1);
  ConnectionManager cm(&context, NULL, 2);
}

void EchoServer(zmq::socket_t *socket) {
  MessageVector v;
  ReadMessageToVector(socket, &v);
  WriteVectorToSocket(socket, v);
  delete socket;
}

boost::thread* StartServer(zmq::context_t* context) {
  zmq::socket_t* server = new zmq::socket_t(*context, ZMQ_DEALER);
  server->bind("inproc://server.test");
  return CreateThread(NewCallback(&EchoServer, server));
}

TEST_F(ConnectionManagerTest, TestSendRequestSync) {
  zmq::context_t context(1);
  scoped_ptr<boost::thread> thread(StartServer(&context));

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

TEST_F(ConnectionManagerTest, TestSendRequestClosure) {
  zmq::context_t context(1);
  scoped_ptr<boost::thread> thread(StartServer(&context));

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  MessageVector request;
  request.push_back(StringToMessage("hello"));
  request.push_back(StringToMessage("there"));
  RemoteResponse response;

  SyncEvent event;
  connection->SendRequest(&request, &response, -1,
                          NewCallback(CheckResponse, &response, &event));
  event.Wait();
  thread->join();
  // Double-check:
  CHECK_EQ(RemoteResponse::DONE, response.status);
  CHECK_EQ(2, response.reply.size());
}

void ExpectTimeout(RemoteResponse* response, SyncEvent* sync) {
  LOG(INFO) << "I am here";
  CHECK_EQ(RemoteResponse::DEADLINE_EXCEEDED, response->status);
  CHECK_EQ(0, response->reply.size());
  sync->Signal();
}

TEST_F(ConnectionManagerTest, TestTimeoutSync) {
  zmq::context_t context(1);
  zmq::socket_t server(context, ZMQ_DEALER);
  server.bind("inproc://server.test");

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  MessageVector request;
  request.push_back(StringToMessage("hello"));
  request.push_back(StringToMessage("there"));
  RemoteResponse response;

  connection->SendRequest(&request, &response, 1, NULL);
  response.Wait();
}

TEST_F(ConnectionManagerTest, TestTimeoutAsync) {
  zmq::context_t context(1);
  zmq::socket_t server(context, ZMQ_DEALER);
  server.bind("inproc://server.test");

  EventManager em(&context, 5);
  ConnectionManager cm(&context, &em, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  MessageVector request;
  request.push_back(StringToMessage("hello"));
  request.push_back(StringToMessage("there"));
  RemoteResponse response;

  SyncEvent event;
  connection->SendRequest(&request, &response, 1,
                          NewCallback(ExpectTimeout, &response, &event));
  event.Wait();
}
}  // namespace zrpc

int main(int argc, char** argv) {
  ::google::InstallFailureSignalHandler();
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = true;
  return RUN_ALL_TESTS();
}
