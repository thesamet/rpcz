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
#include "zrpc/zrpc.h"
#include "cpp/search.pb.h"
#include "cpp/search.zrpc.h"

int main() {
  zrpc::Application application;
  examples::SearchService_Stub search_stub(application.CreateRpcChannel(
          "tcp://localhost:5555"), true);
  examples::SearchRequest request;
  examples::SearchResponse response;
  request.set_query("gold");

  zrpc::RPC rpc;
  rpc.SetDeadlineMs(1000);   // 1 second
  std::cout << "Sending request." << std::endl;
  search_stub.Search(request, &response, &rpc, NULL);

  rpc.Wait();
  std::cout << "Response status: "
            << zrpc::GenericRPCResponse::Status_Name(rpc.GetStatus())
            << std::endl;
  if (rpc.OK()) {
    std::cout << response.DebugString() << std::endl;
  }
}
