// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

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
