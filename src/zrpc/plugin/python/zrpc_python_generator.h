// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: robinson@google.com (Will Robinson)
//
// Generates Python code for a given .proto file.

#ifndef ZRPC_PLUGIN_PYTHON_ZRPC_PYTHON_GENERATOR_H
#define ZRPC_PLUGIN_PYTHON_ZRPC_PYTHON_GENERATOR_H

#include <string>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
class Descriptor;
class EnumDescriptor;
class EnumValueDescriptor;
class FieldDescriptor;
class ServiceDescriptor;
namespace io { class Printer; }
}  // namespace protobuf
}  // namespace google

namespace zrpc {
namespace plugin {
namespace python {

// CodeGenerator implementation for generated Python protocol buffer classes.
// If you create your own protocol compiler binary and you want it to support
// Python output, you can do so by registering an instance of this
// CodeGenerator with the CommandLineInterface in your main() function.
class LIBPROTOC_EXPORT Generator
    : public google::protobuf::compiler::CodeGenerator {
 public:
  Generator();
  virtual ~Generator();

  // CodeGenerator methods.
  virtual bool Generate(
      const google::protobuf::FileDescriptor* file,
      const std::string& parameter,
      google::protobuf::compiler::GeneratorContext* generator_context,
      std::string* error) const;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Generator);
};

class FileGenerator {
 public:
  FileGenerator(const google::protobuf::FileDescriptor* file,
                google::protobuf::io::Printer* printer);

  bool Run();

 private:
  void PrintImports() const;
  void PrintFileDescriptor() const;
  void PrintTopLevelEnums() const;
  void PrintAllNestedEnumsInFile() const;
  void PrintNestedEnums(const google::protobuf::Descriptor& descriptor) const;
  void PrintEnum(const google::protobuf::EnumDescriptor& enum_descriptor) const;

  void PrintTopLevelExtensions() const;

  void PrintFieldDescriptor(
      const google::protobuf::FieldDescriptor& field, bool is_extension) const;
  void PrintFieldDescriptorsInDescriptor(
      const google::protobuf::Descriptor& message_descriptor,
      bool is_extension,
      const std::string& list_variable_name,
      int (google::protobuf::Descriptor::*CountFn)() const,
      const google::protobuf::FieldDescriptor* (google::protobuf::Descriptor::*GetterFn)(int) const) const;
  void PrintFieldsInDescriptor(const google::protobuf::Descriptor& message_descriptor) const;
  void PrintExtensionsInDescriptor(const google::protobuf::Descriptor& message_descriptor) const;
  void PrintMessageDescriptors() const;
  void PrintDescriptor(const google::protobuf::Descriptor& message_descriptor) const;
  void PrintNestedDescriptors(const google::protobuf::Descriptor& containing_descriptor) const;

  void PrintMessages() const;
  void PrintMessage(const google::protobuf::Descriptor& message_descriptor) const;
  void PrintNestedMessages(const google::protobuf::Descriptor& containing_descriptor) const;

  void FixForeignFieldsInDescriptors() const;
  void FixForeignFieldsInDescriptor(
      const google::protobuf::Descriptor& descriptor,
      const google::protobuf::Descriptor* containing_descriptor) const;
  void FixForeignFieldsInField(const google::protobuf::Descriptor* containing_type,
                               const google::protobuf::FieldDescriptor& field,
                               const std::string& python_dict_name) const;
  void AddMessageToFileDescriptor(const google::protobuf::Descriptor& descriptor) const;
  std::string FieldReferencingExpression(const google::protobuf::Descriptor* containing_type,
                                    const google::protobuf::FieldDescriptor& field,
                                    const std::string& python_dict_name) const;
  template <typename DescriptorT>
  void FixContainingTypeInDescriptor(
      const DescriptorT& descriptor,
      const google::protobuf::Descriptor* containing_descriptor) const;

  void FixForeignFieldsInExtensions() const;
  void FixForeignFieldsInExtension(
      const google::protobuf::FieldDescriptor& extension_field) const;
  void FixForeignFieldsInNestedExtensions(const google::protobuf::Descriptor& descriptor) const;

  void PrintServices() const;
  void PrintServiceDescriptor(const google::protobuf::ServiceDescriptor& descriptor) const;
  void PrintServiceClass(const google::protobuf::ServiceDescriptor& descriptor) const;
  void PrintServiceStub(const google::protobuf::ServiceDescriptor& descriptor) const;

  void PrintEnumValueDescriptor(const google::protobuf::EnumValueDescriptor& descriptor) const;
  std::string OptionsValue(const std::string& class_name,
                      const std::string& serialized_options) const;
  bool GeneratingDescriptorProto() const;

  template <typename DescriptorT>
  std::string ModuleLevelDescriptorName(const DescriptorT& descriptor) const;
  std::string ModuleLevelMessageName(const google::protobuf::Descriptor& descriptor) const;
  std::string ModuleLevelServiceDescriptorName(
      const google::protobuf::ServiceDescriptor& descriptor) const;

  template <typename DescriptorT, typename DescriptorProtoT>
  void PrintSerializedPbInterval(
      const DescriptorT& descriptor, DescriptorProtoT& proto) const;

  const google::protobuf::FileDescriptor* file_;
  google::protobuf::io::Printer* printer_;
  std::string file_descriptor_serialized_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FileGenerator);
};
}  // namespace python
}  // namespace plugin
}  // namespace zrpc
#endif  // ZRPC_PLUGIN_PYTHON_ZRPC_PYTHON_GENERATOR_H
