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

#include <iostream>
#include <boost/thread/thread.hpp>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include "proto/search.pb.h"
#include "proto/search.zrpc.h"
#include "zrpc/callback.h"
#include "zrpc/connection_manager.h"
#include "zrpc/event_manager.h"
#include "zrpc/rpc_channel.h"
#include "zrpc/rpc.h"
#include "zrpc/server.h"

using namespace std;

namespace zrpc {

class SearchServiceImpl : public SearchService {
  virtual void Search(
      zrpc::RPC* rpc, const SearchRequest* request,
      SearchResponse* response, Closure* done) {
    if (request->query() == "foo") {
      rpc->SetFailed("I don't like foo.");
    } else if (request->query() == "bar") {
      rpc->SetFailed(17, "I don't like bar.");
    } else {
      response->add_results("The search for " + request->query());
      response->add_results("is great");
    }
    done->Run();
  }
};

void ServerThread(zmq::socket_t* socket, EventManager* em) {
  Server server(socket, em);
  SearchServiceImpl service;
  server.RegisterService(&service);
  server.Start();
}

class ServerTest : public ::testing::Test {
 public:
  ServerTest() :
      context_(new zmq::context_t(1)),
      em_(new EventManager(context_.get(), 10)),
      cm_(new ConnectionManager(context_.get(), em_.get(), 1)) {
  }

  ~ServerTest() {
    // Terminate the context, which will cause the thread to quit.
    em_.reset(NULL);
    cm_.reset(NULL);
    context_.reset(NULL);
    server_thread_.join();
  }

 protected:
  scoped_ptr<zmq::context_t> context_;
  scoped_ptr<EventManager> em_;
  scoped_ptr<ConnectionManager> cm_;
  boost::thread server_thread_;
};

TEST_F(ServerTest, SimpleStart) {
  zmq::socket_t *socket = new zmq::socket_t(*context_, ZMQ_ROUTER);
  socket->bind("inproc://myserver");
  server_thread_ = boost::thread(boost::bind(ServerThread,
                                             socket, em_.get()));
  scoped_ptr<Connection> connection(cm_->Connect("inproc://myserver"));
  SearchService_Stub stub(connection->MakeChannel());
  SearchRequest request;
  SearchResponse response;
  RPC rpc;
  request.set_query("quer");
  stub.Search(&rpc, &request, &response, NULL);
  rpc.Wait();
  CHECK(rpc.OK());
  CHECK_EQ(2, response.results_size());
}
}  // namespace
