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
  FunctionServerTest() : context_(1), fs_(&context_, 10, NULL) {}
 protected:
  zmq::context_t context_;
  FunctionServer fs_;
};

TEST_F(FunctionServerTest, InitializesAndFinishes) {
}

void HandlerFunction(
    SyncEvent* sync_event,
    FunctionServer::ReplyFunction reply) {
  reply(NULL);
  sync_event->Signal();
}

void SendHandler(zmq::socket_t* socket,
                 FunctionServer::HandlerFunction* handler_function) {
  SendEmptyMessage(socket, ZMQ_SNDMORE);
  SendPointer(socket, handler_function, 0);
}

TEST_F(FunctionServerTest, HandlesSingleRequest) {
  // Tests a single request with empty reply.
  SyncEvent event;
  scoped_ptr<zmq::socket_t> socket(fs_.GetConnectedSocket());
  SendHandler(socket.get(), new FunctionServer::HandlerFunction(
          boost::bind(HandlerFunction, &event, _1)));
  event.Wait();
  zmq::message_t msg;
  EXPECT_FALSE(socket->recv(&msg, ZMQ_NOBLOCK));
}

void ReplyingHandlerFunction(
    FunctionServer::ReplyFunction reply) {
  MessageVector reply_vector;
  reply_vector.push_back(StringToMessage("reply1"));
  reply_vector.push_back(StringToMessage("reply2"));
  reply(&reply_vector);
}

TEST_F(FunctionServerTest, HandlesSingleRequestWithResponse) {
  // Tests a single request with one reply.
  scoped_ptr<zmq::socket_t> socket(fs_.GetConnectedSocket());
  SendHandler(socket.get(), 
              new FunctionServer::HandlerFunction(ReplyingHandlerFunction));
  MessageVector reply;
  ReadMessageToVector(socket.get(), &reply);
  CHECK_EQ(3, reply.size());
  EXPECT_EQ("", MessageToString(reply[0]));
  EXPECT_EQ("reply1", MessageToString(reply[1]));
  EXPECT_EQ("reply2", MessageToString(reply[2]));
}

void PropagatingHandlerFunction(FunctionServer* fs,
                                int count,
                                FunctionServer::ReplyFunction reply) {
  if (count < 7) {
    scoped_ptr<zmq::socket_t> socket(fs->GetConnectedSocket());
    SendHandler(socket.get(),
                new FunctionServer::HandlerFunction(boost::bind(
                    PropagatingHandlerFunction,
                    fs, count + 1, _1)));
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
  scoped_ptr<zmq::socket_t> socket(fs_.GetConnectedSocket());
  SendHandler(socket.get(), new FunctionServer::HandlerFunction(
          boost::bind(PropagatingHandlerFunction,
                      &fs_, 0, _1)));
  MessageVector reply_vector;
  ReadMessageToVector(socket.get(), &reply_vector);
  CHECK_EQ(9, reply_vector.size());
  for (int i = 0; i <= 7; ++i) {
    CHECK_EQ(boost::lexical_cast<std::string>(7 - i),
             MessageToString(reply_vector[i+1]));
  }
}

void DelegatingHandlerFunction(FunctionServer* fs,
                               int count,
                               FunctionServer::ReplyFunction* delegated,
                               FunctionServer::ReplyFunction reply) {
  if (count == 0) {
    scoped_ptr<zmq::socket_t> socket(fs->GetConnectedSocket());
    SendHandler(socket.get(),
        new FunctionServer::HandlerFunction(bind(
                DelegatingHandlerFunction,
                fs, 1, &reply, _1)));
    // We pass around a pointer to the ReplyFunction, to be called by the 7'th
    // handler. The reason this pointer remains valid is that we Wait() in
    // this thread.
    MessageVector r;
    CHECK(ReadMessageToVector(socket.get(), &r));
  } else if (count >= 1 && count < 7) {
    scoped_ptr<zmq::socket_t> socket(fs->GetConnectedSocket());
    SendHandler(
        socket.get(),
        new FunctionServer::HandlerFunction(bind(
            DelegatingHandlerFunction, fs, count + 1, delegated,
            _1)));
    MessageVector r;
    CHECK(ReadMessageToVector(socket.get(), &r));
    reply(&r);
  } else if (count == 7) {
    MessageVector reply_vector;
    reply_vector.push_back(StringToMessage("This is it!"));
    (*delegated)(&reply_vector);
    MessageVector r;
    r.push_back(StringToMessage("INNER!"));
    reply(&r);
  } else {
    LOG(FATAL) << "Unexpected to be here";
  }
}

TEST_F(FunctionServerTest, TestDelegatingHandler) {
  // In this test the handlers delegate the responsibility to call the reply
  // function to another reliable. We use an event to make sure the thread that
  // replies is a different thread that received the original function.
  scoped_ptr<zmq::socket_t> socket(fs_.GetConnectedSocket());
  FunctionServer::ReplyFunction ref;
  SendHandler(socket.get(), new FunctionServer::HandlerFunction(
          bind(DelegatingHandlerFunction, &fs_, 0, &ref, _1)));
  MessageVector reply_vector;
  ReadMessageToVector(socket.get(), &reply_vector);
  CHECK_EQ(2, reply_vector.size());
  CHECK_EQ("This is it!", MessageToString(reply_vector[1]));
}
}  // namespace zrpc
