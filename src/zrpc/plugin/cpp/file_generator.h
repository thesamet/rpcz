// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_FILE_GENERATOR_H
#define ZRPC_FILE_GENERATOR_H

#include <string>
#include <vector>

namespace google {
namespace protobuf {
class FileDescriptor;
class ServiceDescriptor;
namespace io {
class Printer;
}
}
}

namespace zrpc {
namespace plugin {
namespace cpp {

class FileGenerator {
  public:
    FileGenerator(const google::protobuf::FileDescriptor* file,
                  const std::string& dllexport_decl);

    void GenerateHeader(google::protobuf::io::Printer* printer);

    void GenerateSource(google::protobuf::io::Printer* printer);

  private:
    void GenerateNamespaceOpeners(google::protobuf::io::Printer* printer);

    void GenerateNamespaceClosers(google::protobuf::io::Printer* printer);

    std::vector<std::string> package_parts_;
    const ::google::protobuf::FileDescriptor* file_;
    std::string dllexport_decl_;
};

}  // namespace cpp
}  // namespace plugin
}  // namespace zrpc
#endif
