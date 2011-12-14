// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <map>
#include <string>
#include <vector>
#include <zmq.hpp>

#include <google/protobuf/descriptor.h>
#include "zrpc/event_manager.h"
#include "zrpc/rpc.h"
#include "zrpc/zrpc.pb.h"
#include "zrpc/service.h"
#include "zrpc/rpc_channel.h"
#include "glog/logging.h"
#include "gflags/gflags.h"

#include "zmq_utils.h"
#include "proto/search.zrpc.h"

void MyCallback(zrpc::RPC* rpc, zrpc::SearchResponse* response) {
  LOG(INFO) << rpc->GetStatus();
  LOG(INFO) << response->DebugString();
}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InstallFailureSignalHandler();
  FLAGS_logtostderr = true;
  {
  zmq::context_t context(1);
  zrpc::EventManager em(&context, 5);

  zrpc::scoped_ptr<zrpc::Connection> connection(
      zrpc::Connection::CreateConnection(
          &em, "tcp://localhost:5556"));

  zrpc::SearchService_Stub stub(connection->MakeChannel(), true);
  zrpc::SearchRequest request;
  request.set_query("Hello");
  zrpc::SearchResponse response;
  zrpc::RPC rpc;
  rpc.SetDeadlineMs(2000);
  stub.Search(&rpc, &request, &response,
              zrpc::NewCallback(&MyCallback, &rpc, &response));
  rpc.Wait();
  LOG(INFO)<<"Wait exited.";
  }
  LOG(INFO) <<"Shutting down";
  google::ShutdownGoogleLogging();
  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}
