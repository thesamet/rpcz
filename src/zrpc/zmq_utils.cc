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

#include "zrpc/zmq_utils.h"
#include <i386/types.h>
#include <stddef.h>
#include <string.h>
#include <zmq.h>
#include <ostream>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "google/protobuf/stubs/common.h"
#include "zmq.hpp"

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
                         MessageVector* data) {
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
                         MessageVector* routes,
                         MessageVector* data) {
  bool first_part = true;
  while (1) {
    zmq::message_t *msg = new zmq::message_t;
    socket->recv(msg, 0);
    int64_t more;           //  Multipart detection
    size_t more_size = sizeof(more);
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
                         const MessageVector& data,
                         int flags) {
  for (size_t i = 0; i < data.size(); ++i) {
    socket->send(*data[i], 
                 flags |
                 ((i < data.size() - 1) ? ZMQ_SNDMORE : 0));
  }
}

void WriteVectorsToSocket(zmq::socket_t* socket,
                          const MessageVector& routes,
                          const MessageVector& data) {
  CHECK_GE(data.size(), 1);
  WriteVectorToSocket(socket, routes, ZMQ_SNDMORE);
  WriteVectorToSocket(socket, data, 0);
}

void SendEmptyMessage(zmq::socket_t* socket,
                      int flags) {
  zmq::message_t message(0);
  socket->send(message, flags);
}

void SendString(zmq::socket_t* socket,
                const std::string& str,
                int flags) {
  scoped_ptr<zmq::message_t> msg(StringToMessage(str));
  socket->send(*msg, flags);
}

void SendUint64(zmq::socket_t* socket,
                google::protobuf::uint64 value,
                int flags) {
  zmq::message_t msg(8);
  memcpy(msg.data(), &value, 8);
  socket->send(msg, flags);
}

bool ForwardMessage(zmq::socket_t &socket_in,
                    zmq::socket_t &socket_out) {
  MessageVector routes;
  MessageVector data;
  CHECK(!ReadMessageToVector(&socket_in, &routes, &data));
  WriteVectorToSocket(&socket_out, routes); 
  return true;
}
}  // namespace
