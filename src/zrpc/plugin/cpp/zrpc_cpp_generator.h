// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_ZRPC_CPP_GENERATOR_H
#define ZRPC_ZRPC_CPP_GENERATOR_H

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/stubs/common.h>
#include <string>

namespace google {
namespace protobuf {
class FileDescriptor;
}  // namespace protobuf
}  // namespace google

namespace zrpc {
namespace plugin {
namespace cpp {

class LIBPROTOC_EXPORT ZRpcCppGenerator :
    public ::google::protobuf::compiler::CodeGenerator {
 public:
  ZRpcCppGenerator();
  ~ZRpcCppGenerator();

  bool Generate(
      const ::google::protobuf::FileDescriptor* file,
      const ::std::string& parameter,
      ::google::protobuf::compiler::GeneratorContext* generator_context,
      std::string* error) const;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ZRpcCppGenerator);
};
}  // namespace cpp
}  // namespace plugin
}  // namespace zrpc
#endif
