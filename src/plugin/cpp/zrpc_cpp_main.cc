// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <google/protobuf/compiler/plugin.h>
#include "zrpc_cpp_generator.h"

int main(int argc, char* argv[]) {
  zrpc::plugin::cpp::ZRpcCppGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
