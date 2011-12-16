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

#ifndef ZRPC_STRING_PIECE_H
#define ZRPC_STRING_PIECE_H

#include <string>

namespace zrpc {

class StringPiece {
  public:
    StringPiece(const char* data, size_t size) : data_(data), size_(size) {}
    StringPiece() : data_(NULL), size_(0) {}
    explicit StringPiece(std::string s) : data_(s.c_str()), size_(s.size()) {}

    const char* data() const { return data_; }
    int size() const { return size_; }
    const std::string as_string() const { return std::string(data(), size()); }

  private:
    const char* data_;
    size_t size_;
};

}  // namespace
#endif
