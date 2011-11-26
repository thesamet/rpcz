find_package(ProtobufPlugin REQUIRED)

protobuf_generate_cpp(ZRPC_PB_SRCS ZRPC_PB_HDRS zrpc.proto)

add_library(zrpc_pb ${ZRPC_PB_SRCS} ${ZRPC_PB_HDRS})
target_link_libraries(zrpc_pb ${PROTOBUF_LIBRARY})
