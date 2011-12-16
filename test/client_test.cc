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

#include <map>
#include <string>
#include <vector>
#include <zmq.hpp>

#include <google/protobuf/descriptor.h>
#include "zrpc/callback.h"
#include "zrpc/connection_manager.h"
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
  zrpc::ConnectionManager em(&context, 5);

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
