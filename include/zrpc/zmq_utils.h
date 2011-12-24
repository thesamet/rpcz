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

#ifndef ZRPC_ZMQ_UTILS_H
#define ZRPC_ZMQ_UTILS_H

#include <string>
#include <boost/ptr_container/ptr_vector.hpp>
#include <glog/logging.h>
#include <google/protobuf/stubs/common.h>
#include <zmq.hpp>

namespace zmq {
class socket_t;
class message_t;
}

namespace zrpc {

class MessageVector {
 public:
  zmq::message_t& operator[](int index) {
    return data_[index];
  }

  size_t size() const { return data_.size(); }

  // transfers points in the the range [from, to) from the other
  // MessageVector to the beginning of this messsage vector.
  void transfer(size_t from, size_t to, MessageVector& other) {
    data_.transfer(data_.begin(),
                   other.data_.begin() + from, other.data_.begin() + to,
                   other.data_);
  }

  template <typename T>
  T begin() {
    return data_.begin();
  }

  void push_back(zmq::message_t* msg) { data_.push_back(msg); }

  void erase_first() { data_.erase(data_.begin()); }

  zmq::message_t* release(int index) {
    return data_.replace(index, NULL).release(); }

 private:
  typedef boost::ptr_vector<boost::nullable<zmq::message_t> > DataType;

  DataType data_;
};

bool ReadMessageToVector(zmq::socket_t* socket,
                         MessageVector* data);

bool ReadMessageToVector(zmq::socket_t* socket,
                         MessageVector* routes,
                         MessageVector* data);

void WriteVectorToSocket(zmq::socket_t* socket,
                         MessageVector& data,
                         int flags=0);

void WriteVectorsToSocket(zmq::socket_t* socket,
                          MessageVector& routes,
                          MessageVector& data);

std::string MessageToString(zmq::message_t& msg);

zmq::message_t* StringToMessage(const std::string& str);

void SendEmptyMessage(zmq::socket_t* socket,
                      int flags=0);

void SendString(zmq::socket_t* socket,
                const std::string& str,
                int flags=0);

void SendUint64(zmq::socket_t* socket,
                google::protobuf::uint64 value,
                int flags=0);

bool ForwardMessage(zmq::socket_t &socket_in,
                    zmq::socket_t &socket_out);

template<class T>
void SendPointer(zmq::socket_t* socket, T* pointer, int flags=0) {
  zmq::message_t msg(sizeof(pointer));
  memcpy(msg.data(), &pointer, sizeof(T*));
  socket->send(msg, flags);
}

template<typename T>
inline T& InterpretMessage(zmq::message_t& msg) {
  CHECK_EQ(msg.size(), sizeof(T));
  T &t = *static_cast<T*>(msg.data());
  return t;
}
}  // namespace zrpc
#endif
