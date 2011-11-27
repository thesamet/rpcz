find_package(ProtobufPlugin REQUIRED)
include_directories($PROTOBUF_INCLUDE_DIR)

set(ZRPC_SRCS zrpc_python_generator.cc zrpc_python_main.cc)
add_executable(zrpc_python_plugin ${ZRPC_SRCS})
target_link_libraries(zrpc_python_plugin ${PROTOBUF_LIBRARY} ${PROTOBUF_PROTOC_LIBRARY})
