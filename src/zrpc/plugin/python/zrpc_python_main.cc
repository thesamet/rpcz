// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "zrpc/plugin/python/zrpc_python_generator.h"
#include <google/protobuf/compiler/plugin.h>

int main(int argc, char* argv[]) {
  zrpc::plugin::python::Generator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
