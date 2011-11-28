project (ZRPC)

# The version number.
set (YOSH_STORE_VERSION_MAJOR 0)
set (YOSH_STORE_VERSION_MINOR 9)

add_subdirectory(plugin)

find_package(ProtobufPlugin REQUIRED)
protobuf_generate_cpp(ZRPC_PB_SRCS ZRPC_PB_HDRS proto/zrpc.proto)
set(PROTO_SOURCES ${ZRPC_PB_SRCS} ${ZRPC_PB_HDRS})

include_directories(${PROJECT_BINARY_DIR})
add_library(zrpc server.cc rpc.cc ${PROTO_SOURCES})

add_executable(zsendrpc zsendrpc.cc)
target_link_libraries(zsendrpc zrpc zmq glog gflags ${PROTOBUF_LIBRARY})
