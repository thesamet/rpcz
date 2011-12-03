// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <map>
#include <string>
#include <vector>
#include <zmq.hpp>

#include "zrpc/event_manager.h"
#include "glog/logging.h"
#include "gflags/gflags.h"

#include "zmq_utils.h"


namespace {
bool ReadMessageToVector(zmq::socket_t* socket,
                         std::vector<zmq::message_t*>* routes,
                         std::vector<zmq::message_t*>* data) {
  bool first_part = true;
  while (1) {
    zmq::message_t *msg = new zmq::message_t;
    socket->recv(msg, 0);
    int64_t more;           //  Multipart detection
    size_t more_size = sizeof (more);
    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (first_part) {
      routes->push_back(msg);
      if (msg->size() == 0) {
        first_part = false;
      }
    } else {
      data->push_back(msg);
    }
    if (!more) {
      return !first_part;
    }
  }
  return !first_part;
}
//  Convert C string to 0MQ string and send to socket
static int
s_send (void *socket, const char *string) {
    int rc;
    zmq_msg_t message;
    zmq_msg_init_size (&message, strlen (string));
    memcpy (zmq_msg_data (&message), string, strlen (string));
    rc = zmq_send (socket, &message, 0);
    zmq_msg_close (&message);
    return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
static int
s_sendmore (void *socket, const char *string) {
    int rc;
    zmq_msg_t message;
    zmq_msg_init_size (&message, strlen (string));
    memcpy (zmq_msg_data (&message), string, strlen (string));
    rc = zmq_send (socket, &message, ZMQ_SNDMORE);
    zmq_msg_close (&message);
    return (rc);
}

static bool
s_dump (const char *client_id, void *socket)
{
    LOG(INFO) << client_id << ": ----------------------------------------";
    while (1) {
        //  Process all parts of the message
        zmq_msg_t message;
        zmq_msg_init (&message);
        int rc;
        if ((rc = zmq_recv (socket, &message, 0)) != 0) {
          LOG(INFO) << client_id << ": error " << zmq_errno();
          return false;
        }

        //  Dump the message as text or binary
        char *data = (char*)zmq_msg_data (&message);
        int size = zmq_msg_size (&message);
        LOG(INFO) << client_id << " [" << size << "]:  "
                  << std::string(data, size);

        int64_t more;           //  Multipart detection
        size_t more_size = sizeof (more);
        zmq_getsockopt (socket, ZMQ_RCVMORE, &more, &more_size);
        zmq_msg_close (&message);
        if (!more)
            break;      //  Last message part
    }
    return true;
}

}

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
  zrpc::EventManager em(&context, 10);
  pthread_t thread;
  {
    zrpc::ClientContext *client_context = new zrpc::ClientContext;
    client_context->context = &context;
    client_context->client_id = "moishe";
    pthread_create(&thread, NULL, zrpc::SimpleClient, client_context);
  }
  usleep(1000);

  zrpc::EventManagerController* controller = em.GetController();
  controller->AddRemoteEndpoint("moishe", "inproc://moishe");
  delete controller;
  usleep(1000);

  zrpc::ClientRequest client_request;
  client_request.status = zrpc::ClientRequest::OK;
  client_request.closure = google::protobuf::NewCallback(
      MyCallback, &client_request);
  
  zmq::socket_t req(context, ZMQ_REQ);
  req.connect("inproc://clients.app");
  s_sendmore(req, "FORWARD");
  s_sendmore(req, "moishe");
  zrpc::SendPointer(&req, &client_request, ZMQ_SNDMORE);
  s_sendmore(req, "lafefon");
  s_send(req, "hamutz");
  
  sleep(5);
  google::ShutdownGoogleLogging();
  return 0;
}
