// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <zmq.hpp>
#include <iostream>
#include <glog/logging.h>
#include <google/gflags.h>
#include "proto/search.pb.h"
#include "server.h"

using namespace std;

class MyRpcController : public ::google::protobuf::RpcController {
};

namespace zrpc {

class SearchServiceImpl : public SearchService {
  virtual void Search(
      ::google::protobuf::RpcController* controller, const SearchRequest* request,
      SearchResponse* response, ::google::protobuf::Closure* done) {
    cerr << "Got message";
    response->add_results("The search");
    response->add_results("is great");
    done->Run();
  }
};

}  // namespace

class ZeroMQServer {
 public:
};

int main(int argc, char **argv) {
  ::google::InitGoogleLogging(argv[0]);
  ::google::ParseCommandLineFlags(&argc, &argv, true);
  ::google::InstallFailureSignalHandler();
  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_REP);
  socket.bind("tcp://*:5555");
  zrpc::Server server(&socket);
  zrpc::SearchServiceImpl search_service;
  server.RegisterService(&search_service);
  server.Start();
  // zrpc::SearchService service;
}

