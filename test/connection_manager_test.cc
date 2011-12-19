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

TEST_F(ConnectionManagerTest, TestSendRequestSync) {
  zmq::context_t context(1);

  zmq::socket_t* server = new zmq::socket_t(context, ZMQ_DEALER);
  server->bind("inproc://server.test");
  boost::thread* thread = CreateThread(NewCallback(&EchoServer, server));

  ConnectionManager cm(&context, NULL, 2);
  scoped_ptr<Connection> connection(cm.Connect("inproc://server.test"));
  MessageVector request;
  request.push_back(StringToMessage("hello"));
  request.push_back(StringToMessage("there"));
  RemoteResponse response;

  connection->SendRequest(&request, &response, -1, NULL);
  CHECK_EQ(0, connection->WaitFor(&response));
  CHECK_EQ(RemoteResponse::DONE, response.status);
  CHECK_EQ(2, response.reply.size());
  CHECK_EQ("hello", MessageToString(response.reply[0]));
  CHECK_EQ("there", MessageToString(response.reply[1]));
}
}  // namespace zrpc

int main(int argc, char** argv) {
  ::google::InstallFailureSignalHandler();
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = true;
  return RUN_ALL_TESTS();
}
