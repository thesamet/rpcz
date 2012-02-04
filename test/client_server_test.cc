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
#include "boost/thread/thread.hpp"
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "zmq.hpp"

#include "rpcz/callback.h"
#include "rpcz/connection_manager.h"
#include "rpcz/event_manager.h"
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
  SearchServiceImpl(SearchService_Stub* backend)
      : backend_(backend), delayed_reply_(NULL) {};

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
      // We lose the request. We are going to reply only when we get a request
      // for the query "delayed".
      delayed_reply_ = reply;
      return;
    } else if (request.query() == "delayed") {
      delayed_reply_.Send(SearchResponse());
      reply.Send(SearchResponse());
    } else {
      SearchResponse response;
      response.add_results("The search for " + request.query());
      response.add_results("is great");
      reply.Send(response);
    }
  }

 private:
  scoped_ptr<SearchService_Stub> backend_;
  Reply<SearchResponse> delayed_reply_;
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

void ServerThread(zmq::socket_t* socket,
                  SearchService *service,
                  EventManager* em) {
  Server server(socket, em);
  server.RegisterService(service);
  server.Start();
  delete service;
}

class ServerTest : public ::testing::Test {
 public:
  ServerTest() :
      context_(new zmq::context_t(1)),
      em_(new EventManager(context_.get(), 10)),
      cm_(new ConnectionManager(context_.get(), em_.get())) {
    StartServer();
  }

  ~ServerTest() {
    // We have to wait until the servers are up before we tear down the event
    // managers (since the servers will try to connect to it).
    // The way we accomplish that here is by sending them a request and
    // waiting for a response.
    SendBlockingRequest(frontend_connection_, "simple");
    SendBlockingRequest(backend_connection_, "simple");
    // Terminate the context, which will cause the thread to quit.
    em_.reset(NULL);
    cm_.reset(NULL);
    context_.reset(NULL);
    server_thread_.join();
    backend_thread_.join();
  }

  void StartServer() {
    zmq::socket_t *backend_socket = new zmq::socket_t(*context_, ZMQ_ROUTER);
    backend_socket->bind("inproc://myserver.backend");
    server_thread_ = boost::thread(
        boost::bind(ServerThread, backend_socket, new BackendSearchServiceImpl,
                    em_.get()));

    backend_connection_ = cm_->Connect("inproc://myserver.backend");
    zmq::socket_t *frontend_socket = new zmq::socket_t(*context_, ZMQ_ROUTER);
    frontend_socket->bind("inproc://myserver.frontend");
    backend_thread_ = boost::thread(
        boost::bind(ServerThread,
                    frontend_socket,
                    new SearchServiceImpl(
                        new SearchService_Stub(
                            RpcChannel::Create(backend_connection_),
                        true)),
                    em_.get()));
    frontend_connection_= cm_->Connect("inproc://myserver.frontend");
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
  scoped_ptr<EventManager> em_;
  scoped_ptr<ConnectionManager> cm_;
  Connection frontend_connection_;
  Connection backend_connection_;
  boost::thread server_thread_;
  boost::thread backend_thread_;
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
  // Now we clean up the closure we kept aside.
  {
    RPC rpc;
    request.set_query("delayed");
    stub.Search(request, &response, &rpc, NULL);
    rpc.Wait();
    ASSERT_TRUE(rpc.OK());
  }
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
  // Now we clean up the closure we kept aside.
  {
    RPC rpc;
    request.set_query("delayed");
    stub.Search(request, &response, &rpc, NULL);
    rpc.Wait();
    ASSERT_TRUE(rpc.OK());
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
  request.set_query("delayed");
  stub.Search(request, &response);
}
}  // namespace
