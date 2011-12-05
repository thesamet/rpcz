// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_ZMQ_UTILS_H
#define ZRPC_ZMQ_UTILS_H

#include "zmq_utils.h"
#include "glog/logging.h"
#include "google/protobuf/stubs/common.h"
#include "zmq.hpp"
#include <vector>
#include <string>

namespace zrpc {
std::string MessageToString(zmq::message_t* msg) {
  return std::string((char*)msg->data(), msg->size());
}

zmq::message_t* StringToMessage(const std::string& str) {
  zmq::message_t* message = new zmq::message_t(str.length());
  memcpy(message->data(), str.c_str(), str.length());
  return message;
}

bool ReadMessageToVector(zmq::socket_t* socket,
                         std::vector<zmq::message_t*>* data) {
  while (1) {
    zmq::message_t *msg = new zmq::message_t;
    socket->recv(msg, 0);
    int64_t more;           //  Multipart detection
    size_t more_size = sizeof (more);
    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    data->push_back(msg);
    if (!more) {
      break;
    }
  }
  return true;
}

bool ReadMessageToVector(zmq::socket_t* socket,
                         std::vector<zmq::message_t*>* routes,
                         std::vector<zmq::message_t*>* data) {
  bool first_part = true;
  while (1) {
    zmq::message_t *msg = new zmq::message_t;
    socket->recv(msg, 0);
    std::string str(MessageToString(msg));
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
}

void WriteVectorToSocket(zmq::socket_t* socket,
                         const std::vector<zmq::message_t*>& data,
                         int flags=0) {
  for (int i = 0; i < data.size(); ++i) {
    socket->send(*data[i], 
                 flags |
                 ((i < data.size() - 1) ? ZMQ_SNDMORE : 0) | flags);
  }
}

void WriteVectorsToSocket(zmq::socket_t* socket,
                          const std::vector<zmq::message_t*>& routes,
                          const std::vector<zmq::message_t*>& data) {
  CHECK_GE(data.size(), 1);
  WriteVectorToSocket(socket, routes, ZMQ_SNDMORE);
  WriteVectorToSocket(socket, data, 0);
}

void SendString(zmq::socket_t* socket,
                const std::string& str,
                int flags=0) {
  zmq::message_t *msg = StringToMessage(str);
  socket->send(*msg, flags);
  delete msg;
}

void SendUint64(zmq::socket_t* socket,
                google::protobuf::uint64 value,
                int flags=0) {
  zmq::message_t msg(8);
  memcpy(msg.data(), &value, 8);
  socket->send(msg, flags);
}

bool ForwardMessage(zmq::socket_t &socket_in,
                    zmq::socket_t &socket_out) {
  std::vector<zmq::message_t*> routes;
  std::vector<zmq::message_t*> data;
  CHECK(!ReadMessageToVector(&socket_in, &routes, &data));
  WriteVectorToSocket(&socket_out, routes); 
  return true;
}
}  // namespace
#endif
