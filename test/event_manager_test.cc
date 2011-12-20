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
#include "zrpc/event_manager.h"
#include <zmq.hpp>
#include "zrpc/callback.h"
#include "zrpc/macros.h"
#include "zrpc/zmq_utils.h"
#include "gtest/gtest.h"
#include "glog/logging.h"


namespace zrpc {

const static char* kEndpoint = "inproc://test";
const static char* kReply = "gotit";

class EventManagerTest : public ::testing::Test {
 public:
  EventManagerTest() {}
 protected:
};

TEST_F(EventManagerTest, StartsAndFinishes) {
  zmq::context_t context(1);
  EventManager em(&context, 3);
}

void DoThis(zmq::context_t* context) {
  LOG(INFO)<<"Creating socket";
  zmq::socket_t socket(*context, ZMQ_PUSH);
  socket.connect(kEndpoint);
  SendString(&socket, kReply);
  socket.close();
  LOG(INFO)<<"socket closed";
}

TEST_F(EventManagerTest, ProcessesSingleCallback) {
  zmq::context_t context(1);
  EventManager em(&context, 10);
  zmq::socket_t socket(context, ZMQ_PULL);
  socket.bind(kEndpoint);
  em.Add(NewCallback(&DoThis, &context));
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

TEST_F(EventManagerTest, ProcessesBroadcast) {
  boost::mutex mu;
  boost::condition_variable cond;
  boost::unique_lock<boost::mutex> lock(mu);
  zmq::context_t context(1);
  EventManager em(&context, 20);
  int x = 0;
  Closure *c = NewPermanentCallback(&Increment, &mu, &cond, &x);
  em.Broadcast(c);
  em.Broadcast(c);
  CHECK_EQ(0, x);  // since we are holding the lock
  while (x != 40) {
    cond.wait(lock);
  }
  delete c;
}

void AddManyClosures(EventManager* em) {
  boost::mutex mu;
  boost::condition_variable cond;
  boost::unique_lock<boost::mutex> lock(mu);
  int x = 0;
  const int kMany = 137;
  for (int i = 0; i < kMany; ++i) {
    em->Add(NewCallback(&Increment, &mu, &cond, &x));
  }
  CHECK_EQ(0, x);  // since we are holding the lock
  while (x != kMany) {
    cond.wait(lock);
  }
}

TEST_F(EventManagerTest, ProcessesManyCallbacksFromManyThreads) {
  zmq::context_t context(1);
  EventManager em(&context, 10);
  const int thread_count = 10;
  boost::thread_group thread_group;
  for (int i = 0; i < thread_count; ++i) {
    thread_group.add_thread(CreateThread(NewCallback(&AddManyClosures,
                                                     &em)));
  }
  thread_group.join_all();
}
}  // namespace zrpc

int main(int argc, char** argv) {
  ::google::InstallFailureSignalHandler();
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = true;
  return RUN_ALL_TESTS();
}
