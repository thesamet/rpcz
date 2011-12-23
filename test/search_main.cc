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

#include <zmq.hpp>
#include <iostream>
#include <glog/logging.h>
#include "gtest/gtest.h"
#include "proto/search.pb.h"
#include "proto/search.zrpc.h"
#include "zrpc/callback.h"
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

class ServerTest : public ::testing::Test {
 public:
  ServerTest() :
      context(1) {}

 protected:
  zmq::context_t context;
};

}  // namespace

int main(int argc, char **argv) {
  ::google::InitGoogleLogging(argv[0]);
  ::google::ParseCommandLineFlags(&argc, &argv, true);
  ::google::InstallFailureSignalHandler();
  /*
  zmq::socket_t socket(context, ZMQ_REP);
  // socket.bind("tcp:// *:5556");
  zrpc::Server server(&socket);
  zrpc::SearchServiceImpl search_service;
  server.RegisterService(&search_service);
  server.Start();
  */
  ::google::protobuf::ShutdownProtobufLibrary();
  ::google::ShutdownGoogleLogging();
}
