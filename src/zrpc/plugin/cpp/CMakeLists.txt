find_package(Protobuf REQUIRED)
include_directories($PROTOBUF_INCLUDE_DIR)

set(ZRPC_SRCS zrpc_cpp_generator.cc file_generator.cc zrpc_cpp_main.cc
              zrpc_cpp_service.cc)
add_executable(zrpc_cpp_plugin ${ZRPC_SRCS})
target_link_libraries(zrpc_cpp_plugin ${PROTOBUF_LIBRARY} ${PROTOBUF_PROTOC_LIBRARY})
