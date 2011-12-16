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
#include "zrpc/connection_manager.h"
#include "zrpc/rpc.h"
#include "zrpc/zrpc.pb.h"
#include "zrpc/service.h"
#include "glog/logging.h"
#include "gflags/gflags.h"

#include "zmq_utils.h"

namespace zrpc {

class Client;

class ClientContext {
 public:
  zmq::context_t *context;
  const char* client_id;
};

void* SimpleClient(void* arg) {
  ClientContext *client_context = static_cast<ClientContext*>(arg);
  zmq::socket_t connection(*client_context->context, ZMQ_REP);
  connection.bind("inproc://moishe");
  while (true) {
    // s_dump(client_context->client_id, connection)) {
    // s_send(connection, "hithere");
    try {
      ForwardMessage(connection, connection);
    } catch (zmq::error_t &e) {
      if (e.num() == ETERM) {
        LOG(INFO) << "Client shutdown";
        return NULL;
      }
    }
    LOG(INFO) << "Replied";
  }
  return NULL;
}

}  // namespace zrpc

void MyCallback(zrpc::ClientRequest *client_request) {
  LOG(INFO) << "Got reply of length " << client_request->result.size();
  LOG(INFO) << zrpc::MessageToString(client_request->result[0]);
  LOG(INFO) << zrpc::MessageToString(client_request->result[1]);
}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InstallFailureSignalHandler();
  FLAGS_logtostderr = true;
  zmq::context_t context(1);
  zrpc::ConnectionManager em(&context, 10);
  pthread_t thread;
  {
    zrpc::ClientContext *client_context = new zrpc::ClientContext;
    client_context->context = &context;
    client_context->client_id = "moishe";
    pthread_create(&thread, NULL, zrpc::SimpleClient, client_context);
  }
  usleep(1000);

  zrpc::ConnectionManagerController* controller = em.GetController();
  controller->AddRemoteEndpoint("moishe", "inproc://moishe");
  delete controller;
  usleep(1000);

  zrpc::ClientRequest client_request;
  client_request.status = zrpc::ClientRequest::OK;
  client_request.closure = google::protobuf::NewCallback(
      MyCallback, &client_request);

  zmq::socket_t req(context, ZMQ_REQ);
  req.connect("inproc://clients.app");
  zrpc::SendString(&req, "FORWARD", ZMQ_SNDMORE);
  zrpc::SendString(&req, "moishe", ZMQ_SNDMORE);
  zrpc::SendPointer(&req, &client_request, ZMQ_SNDMORE);
  zrpc::SendString(&req, "lafefon", ZMQ_SNDMORE);
  zrpc::SendString(&req, "hamutz");

  sleep(5);
  google::ShutdownGoogleLogging();
  return 0;
}
