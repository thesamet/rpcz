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

#include "boost/bind.hpp"
#include "boost/function.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/thread/condition_variable.hpp"
#include "zrpc/function_server.h"
#include "zrpc/sync_event.h"
#include "gtest/gtest.h"

namespace zrpc {

class FunctionServerTest : public ::testing::Test {
 public:
  FunctionServerTest() : context_(1) {}
 protected:
  zmq::context_t context_;
};

TEST_F(FunctionServerTest, InitializesAndFinishes) {
  FunctionServer fs(&context_, 3, FunctionServer::HandlerFunction(),
                    FunctionServer::ThreadInitFunc());
}

void HandlerFunction(
    SyncEvent* sync_event,
    MessageVector* request,
    FunctionServer::ReplyFunction reply) {
  CHECK_EQ(3, request->size());
  EXPECT_EQ("element0", MessageToString((*request)[0]));
  EXPECT_EQ("element1", MessageToString((*request)[1]));
  EXPECT_EQ("element2", MessageToString((*request)[2]));
  reply(NULL);
  sync_event->Signal();
}

TEST_F(FunctionServerTest, HandlesSingleRequest) {
  // Tests a single request with empty reply.
  SyncEvent event;
  FunctionServer fs(&context_, 3, boost::bind(HandlerFunction, &event, _1, _2),
                    FunctionServer::ThreadInitFunc());
  scoped_ptr<zmq::socket_t> socket(fs.GetConnectedSocket());
  SendEmptyMessage(socket.get(), ZMQ_SNDMORE);
  SendString(socket.get(), "element0", ZMQ_SNDMORE);
  SendString(socket.get(), "element1", ZMQ_SNDMORE);
  SendString(socket.get(), "element2", 0);
  event.Wait();
  zmq::message_t msg;
  EXPECT_FALSE(socket->recv(&msg, ZMQ_NOBLOCK));
}

void ReplyingHandlerFunction(
    MessageVector* request,
    FunctionServer::ReplyFunction reply) {
  CHECK_EQ(3, request->size());
  EXPECT_EQ("element0", MessageToString((*request)[0]));
  EXPECT_EQ("element1", MessageToString((*request)[1]));
  EXPECT_EQ("element2", MessageToString((*request)[2]));
  MessageVector reply_vector;
  reply_vector.push_back(StringToMessage("reply1"));
  reply_vector.push_back(StringToMessage("reply2"));
  reply(&reply_vector);
}

TEST_F(FunctionServerTest, HandlesSingleRequestWithResponse) {
  // Tests a single request with one reply.
  FunctionServer fs(&context_, 3, ReplyingHandlerFunction,
                    FunctionServer::ThreadInitFunc());
  scoped_ptr<zmq::socket_t> socket(fs.GetConnectedSocket());
  SendEmptyMessage(socket.get(), ZMQ_SNDMORE);
  SendString(socket.get(), "element0", ZMQ_SNDMORE);
  SendString(socket.get(), "element1", ZMQ_SNDMORE);
  SendString(socket.get(), "element2", 0);
  MessageVector reply;
  ReadMessageToVector(socket.get(), &reply);
  CHECK_EQ(3, reply.size());
  EXPECT_EQ("", MessageToString(reply[0]));
  EXPECT_EQ("reply1", MessageToString(reply[1]));
  EXPECT_EQ("reply2", MessageToString(reply[2]));
}

void PropagatingHandlerFunction(FunctionServer* fs,
                                MessageVector* request,
                                FunctionServer::ReplyFunction reply) {
  CHECK_EQ(1, request->size());
  int count = boost::lexical_cast<int>(MessageToString((*request)[0]));
  if (count < 7) {
    scoped_ptr<zmq::socket_t> socket(fs->GetConnectedSocket());
    SendEmptyMessage(socket.get(), ZMQ_SNDMORE);
    SendString(socket.get(), boost::lexical_cast<std::string>(count + 1));
    MessageVector reply_vector;
    ReadMessageToVector(socket.get(), &reply_vector);
    socket->close();
    reply_vector.push_back(StringToMessage(
            boost::lexical_cast<std::string>(count)));
    reply_vector.erase(0);
    reply(&reply_vector);
  }
  if (count == 7) {
    MessageVector reply_vector;
    reply_vector.push_back(StringToMessage("7"));
    reply(&reply_vector);
  }
}

TEST_F(FunctionServerTest, HandlerPropagates) {
  // In this test the handlers send their own message to the same fs,
  // wait for the reply, and based on that reply to the original request.
  FunctionServer fs(&context_, 10, 
                    boost::bind(PropagatingHandlerFunction,
                                &fs, _1, _2),
                    FunctionServer::ThreadInitFunc());
  scoped_ptr<zmq::socket_t> socket(fs.GetConnectedSocket());
  SendEmptyMessage(socket.get(), ZMQ_SNDMORE);
  SendString(socket.get(), "0", 0);
  MessageVector reply_vector;
  ReadMessageToVector(socket.get(), &reply_vector);
  CHECK_EQ(9, reply_vector.size());
  for (int i = 0; i <= 7; ++i) {
    CHECK_EQ(boost::lexical_cast<std::string>(7 - i),
             MessageToString(reply_vector[i+1]));
  }
}

void DelegatingHandlerFunction(SyncEvent* sync_event,
                               FunctionServer* fs,
                               MessageVector* request,
                               FunctionServer::ReplyFunction reply) {
  CHECK_GE(request->size(), 1);
  int count = boost::lexical_cast<int>(MessageToString((*request)[0]));
  LOG(INFO)<<"Count=" << count;
  if (count == 0) {
    scoped_ptr<zmq::socket_t> socket(fs->GetConnectedSocket());
    SendEmptyMessage(socket.get(), ZMQ_SNDMORE);
    SendString(socket.get(), "1", ZMQ_SNDMORE);
    // We pass around a pointer to the ReplyFunction, to be called by the 7'th
    // handler. The reason this point remains valid is that we Wait() in this
    // thread.
    SendPointer<FunctionServer::ReplyFunction>(socket.get(), &reply, 0);
    sync_event->Wait();
  } else if (count >= 1 && count < 7) {
    scoped_ptr<zmq::socket_t> socket(fs->GetConnectedSocket());
    SendEmptyMessage(socket.get(), ZMQ_SNDMORE);
    SendString(socket.get(), boost::lexical_cast<std::string>(count + 1),
               ZMQ_SNDMORE);
    socket->send(*(*request)[1]);
    sync_event->Wait();
    reply(NULL);
  } else if (count == 7) {
    MessageVector reply_vector;
    reply_vector.push_back(StringToMessage("This is it!"));
    (*InterpretMessage<FunctionServer::ReplyFunction*>(*(*request)[1]))(&reply_vector);
    reply(NULL);
    sync_event->Signal();
  } else {
    LOG(FATAL) << "Unexpected to be here";
  }
}

TEST_F(FunctionServerTest, TestDelegatingHandler) {
  // In this test the handlers delegate the responsibility to call the reply
  // function to another reliable. We use an event to make sure the thread that
  // replies is a different thread that received the original function.
  SyncEvent event;
  FunctionServer fs(&context_, 10, 
                    boost::bind(DelegatingHandlerFunction,
                                &event, &fs, _1, _2),
                    FunctionServer::ThreadInitFunc());
  scoped_ptr<zmq::socket_t> socket(fs.GetConnectedSocket());
  SendEmptyMessage(socket.get(), ZMQ_SNDMORE);
  SendString(socket.get(), "0", 0);
  MessageVector reply_vector;
  ReadMessageToVector(socket.get(), &reply_vector);
  CHECK_EQ(2, reply_vector.size());
  CHECK_EQ("This is it!", MessageToString(reply_vector[1]));
}
}  // namespace zrpc
