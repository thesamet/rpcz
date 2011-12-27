find_package(ProtobufPlugin REQUIRED)
include_directories($PROTOBUF_INCLUDE_DIR)

set(ZRPC_SRCS zrpc_cpp_generator.cc file_generator.cc zrpc_cpp_main.cc
              zrpc_cpp_service.cc)
add_executable(protoc-gen-cpp_zrpc ${ZRPC_SRCS})
target_link_libraries(protoc-gen-cpp_zrpc ${PROTOBUF_LIBRARY} ${PROTOBUF_PROTOC_LIBRARY})
