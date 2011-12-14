// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <zmq.hpp>
#include <iostream>
#include <glog/logging.h>
#include <google/gflags.h>
#include "proto/search.pb.h"
#include "proto/search.zrpc.h"
#include "zrpc/rpc.h"
#include "zrpc/server.h"

using namespace std;

namespace zrpc {

class SearchServiceImpl : public SearchService {
  virtual void Search(
      zrpc::RPC* rpc, const SearchRequest* request,
      SearchResponse* response, ::google::protobuf::Closure* done) {
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

}  // namespace

int main(int argc, char **argv) {
  ::google::InitGoogleLogging(argv[0]);
  ::google::ParseCommandLineFlags(&argc, &argv, true);
  ::google::InstallFailureSignalHandler();
  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_REP);
  socket.bind("tcp://*:5556");
  zrpc::Server server(&socket);
  zrpc::SearchServiceImpl search_service;
  server.RegisterService(&search_service);
  server.Start();
  ::google::protobuf::ShutdownProtobufLibrary();
  ::google::ShutdownGoogleLogging();
}
