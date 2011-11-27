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
// This module outputs pure-Python protocol message classes that will
// largely be constructed at runtime via the metaclass in reflection.py.
// In other words, our job is basically to output a Python equivalent
// of the C++ *Descriptor objects, and fix up all circular references
// within these objects.
//
// Note that the runtime performance of protocol message classes created in
// this way is expected to be lousy.  The plan is to create an alternate
// generator that outputs a Python/C extension module that lets
// performance-minded Python code leverage the fast C++ implementation
// directly.

#include "zrpc/plugin/python/zrpc_python_generator.h"

#include <google/protobuf/descriptor.pb.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include "zrpc/plugin/common/strutil.h"
#include <zrpc/plugin/common/substitute.h>

namespace zrpc {
namespace plugin {
namespace python {

namespace {

using std::string;
using namespace google::protobuf;
using namespace google::protobuf::compiler;
// Returns a copy of |filename| with any trailing ".protodevel" or ".proto
// suffix stripped.
// TODO(robinson): Unify with copy in compiler/cpp/internal/helpers.cc.
string StripProto(const string& filename) {
  const char* suffix = HasSuffixString(filename, ".protodevel")
      ? ".protodevel" : ".proto";
  return StripSuffixString(filename, suffix);
}


// Returns the Python module name expected for a given .proto filename.
string ModuleName(const string& filename) {
  string basename = StripProto(filename);
  StripString(&basename, "-", '_');
  StripString(&basename, "/", '.');
  return basename + "_pb2";
}

// Returns the ZRPC Python module name expected for a given .proto filename.
string ZRPCModuleName(const string& filename) {
  string basename = StripProto(filename);
  StripString(&basename, "-", '_');
  StripString(&basename, "/", '.');
  return basename + "_zrpc";
}

// Returns the name of all containing types for descriptor,
// in order from outermost to innermost, followed by descriptor's
// own name.  Each name is separated by |separator|.
template <typename DescriptorT>
string NamePrefixedWithNestedTypes(const DescriptorT& descriptor,
                                   const string& separator) {
  string name = descriptor.name();
  for (const Descriptor* current = descriptor.containing_type();
       current != NULL; current = current->containing_type()) {
    name = current->name() + separator + name;
  }
  return name;
}

// Name of the class attribute where we store the Python
// descriptor.Descriptor instance for the generated class.
// Must stay consistent with the _DESCRIPTOR_KEY constant
// in proto2/public/reflection.py.
const char kDescriptorKey[] = "DESCRIPTOR";


// Should we generate generic services for this file?
inline bool HasGenericServices(const FileDescriptor *file) {
  return file->service_count() > 0 &&
         file->options().py_generic_services();
}


// Prints the common boilerplate needed at the top of every .py
// file output by this generator.
void PrintTopBoilerplate(
    io::Printer* printer, const FileDescriptor* file, bool descriptor_proto) {
  // TODO(robinson): Allow parameterization of Python version?
  printer->Print(
      "# Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
      "\n"
      "from google.protobuf import descriptor\n"
      "from zrpc import service\n"
      "from zrpc import service_reflection\n");

  // Avoid circular imports if this module is descriptor_pb2.
  if (!descriptor_proto) {
    printer->Print(
        "from google.protobuf import descriptor_pb2\n");
  }
  printer->Print(
    "# @@protoc_insertion_point(imports)\n");
  printer->Print("\n\n");
}

}  // namespace


Generator::Generator() { }

Generator::~Generator() {
}

bool Generator::Generate(const FileDescriptor* file,
                         const string& parameter,
                         GeneratorContext* context,
                         string* error) const {
  string module_name = ZRPCModuleName(file->name());
  string filename = module_name;
  StripString(&filename, ".", '/');
  filename += ".py";

  scoped_ptr<io::ZeroCopyOutputStream> output(context->Open(filename));
  GOOGLE_CHECK(output.get());
  io::Printer printer(output.get(), '$');

  FileGenerator file_generator(file, &printer);
  return file_generator.Run();
}

FileGenerator::FileGenerator(const FileDescriptor* file,
                             io::Printer* printer)
    : file_(file), printer_(printer) {
}

bool FileGenerator::Run() {
  FileDescriptorProto fdp;
  file_->CopyTo(&fdp);
  fdp.SerializeToString(&file_descriptor_serialized_);

  PrintTopBoilerplate(printer_, file_, GeneratingDescriptorProto());
  PrintImports();
  PrintServices();
  return !printer_->failed();
}

// Prints Python imports for all modules imported by |file|.
void FileGenerator::PrintImports() const {
  printer_->Print("import $module$\n", "module", ModuleName(file_->name()));
  for (int i = 0; i < file_->dependency_count(); ++i) {
    string module_name = ModuleName(file_->dependency(i)->name());
    printer_->Print("import $module$\n", "module",
                    module_name);
  }
  printer_->Print("\n");
}

void FileGenerator::PrintServices() const {
  for (int i = 0; i < file_->service_count(); ++i) {
    PrintServiceDescriptor(*file_->service(i));
    PrintServiceClass(*file_->service(i));
    PrintServiceStub(*file_->service(i));
    printer_->Print("\n");
  }
}

void FileGenerator::PrintServiceDescriptor(
    const ServiceDescriptor& descriptor) const {
  printer_->Print("\n");
  string service_name = ModuleLevelServiceDescriptorName(descriptor);
  string options_string;
  descriptor.options().SerializeToString(&options_string);

  printer_->Print(
      "$service_name$ = descriptor.ServiceDescriptor(\n",
      "service_name", service_name);
  printer_->Indent();
  map<string, string> m;
  m["name"] = descriptor.name();
  m["full_name"] = descriptor.full_name();
  m["file"] = kDescriptorKey;
  m["index"] = SimpleItoa(descriptor.index());
  m["options_value"] = OptionsValue("ServiceOptions", options_string);
  const char required_function_arguments[] =
      "name='$name$',\n"
      "full_name='$full_name$',\n"
      "file=$file$,\n"
      "index=$index$,\n"
      "options=$options_value$,\n";
  printer_->Print(m, required_function_arguments);

  ServiceDescriptorProto sdp;
  PrintSerializedPbInterval(descriptor, sdp);

  printer_->Print("methods=[\n");
  for (int i = 0; i < descriptor.method_count(); ++i) {
    const MethodDescriptor* method = descriptor.method(i);
    string options_string;
    method->options().SerializeToString(&options_string);

    m.clear();
    m["name"] = method->name();
    m["full_name"] = method->full_name();
    m["index"] = SimpleItoa(method->index());
    m["serialized_options"] = CEscape(options_string);
    m["input_type"] = ModuleLevelDescriptorName(*(method->input_type()));
    m["output_type"] = ModuleLevelDescriptorName(*(method->output_type()));
    m["options_value"] = OptionsValue("MethodOptions", options_string);
    printer_->Print("descriptor.MethodDescriptor(\n");
    printer_->Indent();
    printer_->Print(
        m,
        "name='$name$',\n"
        "full_name='$full_name$',\n"
        "index=$index$,\n"
        "containing_service=None,\n"
        "input_type=$input_type$,\n"
        "output_type=$output_type$,\n"
        "options=$options_value$,\n");
    printer_->Outdent();
    printer_->Print("),\n");
  }

  printer_->Outdent();
  printer_->Print("])\n\n");
}

void FileGenerator::PrintServiceClass(const ServiceDescriptor& descriptor) const {
  // Print the service.
  printer_->Print("class $class_name$(service.Service):\n",
                  "class_name", descriptor.name());
  printer_->Indent();
  printer_->Print(
      "__metaclass__ = service_reflection.GeneratedServiceType\n"
      "$descriptor_key$ = $descriptor_name$\n",
      "descriptor_key", kDescriptorKey,
      "descriptor_name", ModuleLevelServiceDescriptorName(descriptor));
  printer_->Outdent();
}

void FileGenerator::PrintServiceStub(const ServiceDescriptor& descriptor) const {
  // Print the service stub.
  printer_->Print("class $class_name$_Stub($class_name$):\n",
                  "class_name", descriptor.name());
  printer_->Indent();
  printer_->Print(
      "__metaclass__ = service_reflection.GeneratedServiceStubType\n"
      "$descriptor_key$ = $descriptor_name$\n",
      "descriptor_key", kDescriptorKey,
      "descriptor_name", ModuleLevelServiceDescriptorName(descriptor));
  printer_->Outdent();
}

// Returns a Python expression that calls descriptor._ParseOptions using
// the given descriptor class name and serialized options protobuf string.
string FileGenerator::OptionsValue(
    const string& class_name, const string& serialized_options) const {
  if (serialized_options.length() == 0 || GeneratingDescriptorProto()) {
    return "None";
  } else {
    string full_class_name = "descriptor_pb2." + class_name;
    return "descriptor._ParseOptions(" + full_class_name + "(), '"
        + CEscape(serialized_options)+ "')";
  }
}

bool FileGenerator::GeneratingDescriptorProto() const {
  return file_->name() == "google/protobuf/descriptor.proto";
}

// Returns the unique Python module-level identifier given to a descriptor.
// This name is module-qualified iff the given descriptor describes an
// entity that doesn't come from the current file.
template <typename DescriptorT>
string FileGenerator::ModuleLevelDescriptorName(
    const DescriptorT& descriptor) const {
  // FIXME(robinson):
  // We currently don't worry about collisions with underscores in the type
  // names, so these would collide in nasty ways if found in the same file:
  //   OuterProto.ProtoA.ProtoB
  //   OuterProto_ProtoA.ProtoB  # Underscore instead of period.
  // As would these:
  //   OuterProto.ProtoA_.ProtoB
  //   OuterProto.ProtoA._ProtoB  # Leading vs. trailing underscore.
  // (Contrived, but certainly possible).
  //
  // The C++ implementation doesn't guard against this either.  Leaving
  // it for now...
  string name = NamePrefixedWithNestedTypes(descriptor, "_");
  UpperString(&name);
  // Module-private for now.  Easy to make public later; almost impossible
  // to make private later.
  name = "_" + name;
  // We now have the name relative to its own module.  Also qualify with
  // the module name iff this descriptor is from a different .proto file.
  if (descriptor.file() != file_) {
    name = ModuleName(descriptor.file()->name()) + "." + name;
  }
  return name;
}

// Returns the unique Python module-level identifier given to a service
// descriptor.
string FileGenerator::ModuleLevelServiceDescriptorName(
    const ServiceDescriptor& descriptor) const {
  string name = descriptor.name();
  UpperString(&name);
  name = "_" + name;
  if (descriptor.file() != file_) {
    name = ModuleName(descriptor.file()->name()) + "." + name;
  }
  return name;
}

// Prints standard constructor arguments serialized_start and serialized_end.
// Args:
//   descriptor: The cpp descriptor to have a serialized reference.
//   proto: A proto
// Example printer output:
// serialized_start=41,
// serialized_end=43,
//
template <typename DescriptorT, typename DescriptorProtoT>
void FileGenerator::PrintSerializedPbInterval(
    const DescriptorT& descriptor, DescriptorProtoT& proto) const {
  descriptor.CopyTo(&proto);
  string sp;
  proto.SerializeToString(&sp);
  int offset = file_descriptor_serialized_.find(sp);
  GOOGLE_CHECK_GE(offset, 0);

  printer_->Print("serialized_start=$serialized_start$,\n"
                  "serialized_end=$serialized_end$,\n",
                  "serialized_start", SimpleItoa(offset),
                  "serialized_end", SimpleItoa(offset + sp.size()));
}
}  // namespace python
}  // namespace plugin
}  // namespace zrpc
