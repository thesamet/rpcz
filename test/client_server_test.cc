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

#include "rpcz/callback.h"
#include "rpcz/connection_manager.h"
#include "rpcz/rpc_channel.h"
#include "rpcz/rpc.h"
#include "rpcz/server.h"
#include "rpcz/sync_event.h"

#include "proto/search.pb.h"
#include "proto/search.rpcz.h"

using namespace std;

namespace rpcz {

void SuperDone(SearchResponse *response,
               RPC* newrpc, Reply<SearchResponse> reply) {
  delete newrpc;
  reply.Send(*response);
  delete response;
}

class SearchServiceImpl : public SearchService {
 public:
  SearchServiceImpl(SearchService_Stub* backend, ConnectionManager* cm)
      : backend_(backend), delayed_reply_(NULL), cm_(cm) {};

  ~SearchServiceImpl() {
  }

  virtual void Search(
      const SearchRequest& request,
      Reply<SearchResponse> reply) {
    if (request.query() == "foo") {
      reply.Error(-4, "I don't like foo.");
    } else if (request.query() == "bar") {
      reply.Error(17, "I don't like bar.");
    } else if (request.query() == "delegate") {
      RPC* newrpc = new RPC;
      SearchResponse* response = new SearchResponse;
      backend_->Search(request, response, newrpc, NewCallback(SuperDone,
                                                              response,
                                                              newrpc,
                                                              reply));
      return;
    } else if (request.query() == "timeout") {
      // We "lose" the request. We are going to reply only when we get a request
      // for the query "delayed".
      boost::unique_lock<boost::mutex> lock(mu_);
      delayed_reply_ = reply;
      timeout_request_received.Signal();
      return;
    } else if (request.query() == "delayed") {
      boost::unique_lock<boost::mutex> lock(mu_);
      delayed_reply_.Send(SearchResponse());
      reply.Send(SearchResponse());
    } else if (request.query() == "terminate") {
      reply.Send(SearchResponse());
      cm_->Terminate();
    } else {
      SearchResponse response;
      response.add_results("The search for " + request.query());
      response.add_results("is great");
      reply.Send(response);
    }
  }

  SyncEvent timeout_request_received;

 private:
  scoped_ptr<SearchService_Stub> backend_;
  boost::mutex mu_;
  Reply<SearchResponse> delayed_reply_;
  ConnectionManager* cm_;
};

// For handling complex delegated queries.
class BackendSearchServiceImpl : public SearchService {
 public:
  virtual void Search(
      const SearchRequest&,
      Reply<SearchResponse> reply) {
    SearchResponse response;
    response.add_results("42!");
    reply.Send(response);
  }
};

class ServerTest : public ::testing::Test {
 public:
  ServerTest() :
      context_(new zmq::context_t(1)),
      cm_(new ConnectionManager(context_.get(), 10)),
      frontend_server_(cm_.get()),
      backend_server_(cm_.get()) {
    StartServer();
  }

  ~ServerTest() {
    // Terminate the context, which will cause the thread to quit.
    cm_.reset(NULL);
    context_.reset(NULL);
  }

  void StartServer() {
    backend_server_.RegisterService(
        new BackendSearchServiceImpl);
    backend_server_.Bind("inproc://myserver.backend");
    backend_connection_ = cm_->Connect("inproc://myserver.backend");

    frontend_server_.RegisterService(
        frontend_service = new SearchServiceImpl(
            new SearchService_Stub(
                RpcChannel::Create(backend_connection_), true), cm_.get()));
    frontend_server_.Bind("inproc://myserver.frontend");
    frontend_connection_ = cm_->Connect("inproc://myserver.frontend");
  }

  SearchResponse SendBlockingRequest(Connection connection,
                                     const std::string& query) {
    SearchService_Stub stub(RpcChannel::Create(connection), true);
    SearchRequest request;
    SearchResponse response;
    request.set_query(query);
    RPC rpc;
    stub.Search(request, &response, &rpc, NULL);
    rpc.Wait();
    EXPECT_TRUE(rpc.OK());
    return response;
  }

 protected:
  scoped_ptr<zmq::context_t> context_;
  scoped_ptr<ConnectionManager> cm_;
  Connection frontend_connection_;
  Connection backend_connection_;
  Server frontend_server_;
  Server backend_server_;
  SearchServiceImpl* frontend_service;
};

TEST_F(ServerTest, SimpleRequest) {
  SearchResponse response =
      SendBlockingRequest(frontend_connection_, "happiness");
  ASSERT_EQ(2, response.results_size());
  ASSERT_EQ("The search for happiness", response.results(0));
}

TEST_F(ServerTest, SimpleRequestAsync) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  SearchResponse response;
  RPC rpc;
  request.set_query("happiness");
  SyncEvent sync;
  stub.Search(request, &response, &rpc, NewCallback(
          &sync, &SyncEvent::Signal));
  sync.Wait();
  ASSERT_TRUE(rpc.OK());
  ASSERT_EQ(2, response.results_size());
  ASSERT_EQ("The search for happiness", response.results(0));
}

TEST_F(ServerTest, SimpleRequestWithError) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  request.set_query("foo");
  SearchResponse response;
  RPC rpc;
  stub.Search(request, &response, &rpc, NULL);
  rpc.Wait();
  ASSERT_EQ(RpcResponseHeader::APPLICATION_ERROR, rpc.GetStatus());
  ASSERT_EQ("I don't like foo.", rpc.GetErrorMessage());
}

TEST_F(ServerTest, SimpleRequestWithTimeout) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  SearchResponse response;
  RPC rpc;
  request.set_query("timeout");
  rpc.SetDeadlineMs(1);
  stub.Search(request, &response, &rpc, NULL);
  rpc.Wait();
  ASSERT_EQ(RpcResponseHeader::DEADLINE_EXCEEDED, rpc.GetStatus());
}

TEST_F(ServerTest, SimpleRequestWithTimeoutAsync) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  SearchResponse response;
  {
    RPC rpc;
    request.set_query("timeout");
    rpc.SetDeadlineMs(1);
    SyncEvent event;
    stub.Search(request, &response, &rpc,
                NewCallback(&event, &SyncEvent::Signal));
    event.Wait();
    ASSERT_EQ(RpcResponseHeader::DEADLINE_EXCEEDED, rpc.GetStatus());
  }
}

TEST_F(ServerTest, DelegatedRequest) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  SearchResponse response;
  RPC rpc;
  request.set_query("delegate");
  stub.Search(request, &response, &rpc, NULL);
  rpc.Wait();
  ASSERT_EQ(RpcResponseHeader::OK, rpc.GetStatus());
  ASSERT_EQ("42!", response.results(0));
}

TEST_F(ServerTest, EasyBlockingRequestUsingDelegate) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  SearchResponse response;
  request.set_query("delegate");
  stub.Search(request, &response);
  ASSERT_EQ("42!", response.results(0));
}

TEST_F(ServerTest, EasyBlockingRequestRaisesExceptions) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  SearchResponse response;
  request.set_query("foo");
  try {
    stub.Search(request, &response);
    ASSERT_TRUE(false);
  } catch (RpcError &error) {
    ASSERT_EQ(status::APPLICATION_ERROR, error.GetStatus());
    ASSERT_EQ(-4, error.GetApplicationErrorCode());
  }
}

TEST_F(ServerTest, EasyBlockingRequestWithTimeout) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  SearchResponse response;
  request.set_query("timeout");
  try {
    stub.Search(request, &response, 1);
    ASSERT_TRUE(false);
  } catch (RpcError &error) {
    ASSERT_EQ(status::DEADLINE_EXCEEDED, error.GetStatus());
  }
  // We may get here before the timing out request was processed, and if we
  // just send delay right away, the server may be unable to reply.
  frontend_service->timeout_request_received.Wait();
  request.set_query("delayed");
  stub.Search(request, &response);
}

TEST_F(ServerTest, ConnectionManagerTermination) {
  SearchService_Stub stub(RpcChannel::Create(frontend_connection_), true);
  SearchRequest request;
  request.set_query("terminate");
  SearchResponse response;
  try {
    stub.Search(request, &response, 1);
  } catch (RpcError &error) {
    ASSERT_EQ(status::DEADLINE_EXCEEDED, error.GetStatus());
  }
  LOG(INFO)<<"I'm here";
  cm_->Run();
  LOG(INFO)<<"I'm there";
}
}  // namespace
