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

#ifndef RPCZ_ZMQ_UTILS_H
#define RPCZ_ZMQ_UTILS_H

#include <string>
#include "boost/ptr_container/ptr_vector.hpp"
#include "boost/move/move.hpp"
#include "zmq.hpp"
#include "rpcz/macros.h"

namespace zmq {
class socket_t;
class message_t;
}

namespace rpcz {

class zmq_message : public zmq::message_t {
 public:
  zmq_message() : zmq::message_t() {}

  explicit zmq_message(size_t size) : zmq::message_t(size) {}

  // Copy constructor makes hard copy
  zmq_message(const zmq_message& other) : zmq::message_t(other.size()) {
    memcpy(data(), other.data(), other.size());
  }

  // Copy assignment (hard copy)
  inline zmq_message& operator=(BOOST_COPY_ASSIGN_REF(zmq_message) other) {
    if (this != &other) {
      size_t size = other.size();
      zmq::message_t new_msg(size);
      memcpy(new_msg.data(), other.data(), size);
      move(&new_msg);
    }
    return *this;
  }

  // Move constructor
  zmq_message(BOOST_RV_REF(zmq_message) other) {
    move(&other);
  }

  // Move assignment
  zmq_message& operator=(BOOST_RV_REF(zmq_message) other) {
    if (this != &other) {
      move(&other);
    }
    return *this;
  }

  inline void* data() {
    return zmq::message_t::data();
  }

  inline const void* data() const {
    return const_cast<zmq_message*>(this)->data();
  }

  inline size_t size() const {
    return static_cast<zmq::message_t*>(
        const_cast<zmq_message*>(this))->size();
  }

  std::string ToString() const {
    std::string s(static_cast<const char*>(data()), size());
    return s;
  }

  static zmq_message FromString(const std::string& str) {
    zmq_message msg(str.size());
    str.copy(static_cast<char*>(msg.data()), str.size(), 0);
    return msg;
  }

 private:
  zmq_msg_t msg_;
  BOOST_COPYABLE_AND_MOVABLE(zmq_message);
};

class MessageIterator {
 public:
  explicit MessageIterator(zmq::socket_t& socket) :
      socket_(socket), has_more_(true), more_size_(sizeof(has_more_)) { };

  MessageIterator(const MessageIterator& other) :
      socket_(other.socket_),
      has_more_(other.has_more_),
      more_size_(other.more_size_) {
  }

  ~MessageIterator() {
    while (has_more()) next();
  }

  inline bool has_more() { return has_more_; }

  inline zmq_message& next() {
    socket_.recv(&message_, 0);
    socket_.getsockopt(ZMQ_RCVMORE, &has_more_, &more_size_);
    return message_;
  }

 private:
  zmq::socket_t& socket_;
  zmq_message message_;
  int64_t has_more_;
  size_t more_size_;

  MessageIterator& operator=(const MessageIterator&);
};

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

bool SendEmptyMessage(zmq::socket_t* socket,
                      int flags=0);

bool SendString(zmq::socket_t* socket,
                const std::string& str,
                int flags=0);

bool SendUint64(zmq::socket_t* socket,
                uint64 value,
                int flags=0);

bool ForwardMessage(zmq::socket_t &socket_in,
                    zmq::socket_t &socket_out);

template<typename T, typename Message>
inline T& InterpretMessage(Message& msg) {
  assert(msg.size() == sizeof(T));
  T &t = *static_cast<T*>(msg.data());
  return t;
}

template<typename T>
inline bool SendPointer(zmq::socket_t* socket, T* ptr, int flags=0) {
  zmq::message_t msg(sizeof(T*));
  memcpy(msg.data(), &ptr, sizeof(T*));
  return socket->send(msg, flags);
}

inline bool SendChar(zmq::socket_t* socket, char ch, int flags=0) {
  zmq::message_t msg(1);
  *(char*)msg.data() = ch;
  return socket->send(msg, flags);
}

void LogMessageVector(MessageVector& vector);
}  // namespace rpcz
#endif
