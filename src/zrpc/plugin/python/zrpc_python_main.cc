// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include <google/protobuf/compiler/plugin.h>
#include "python_generator.h"

int main(int argc, char* argv[]) {
  zrpc::plugin::python::ZRpcCppGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
