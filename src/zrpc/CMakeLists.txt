project (ZRPC)

# The version number.
set (ZRPC_VERSION_MAJOR 0)
set (ZRPC_VERSION_MINOR 9)

add_subdirectory(plugin)

protobuf_generate_cpp(ZRPC_PB_SRCS ZRPC_PB_HDRS proto/zrpc.proto)
set(PROTO_SOURCES ${ZRPC_PB_SRCS} ${ZRPC_PB_HDRS})

include_directories(${PROJECT_BINARY_DIR})
add_library(zrpc clock.cc server.cc connection_manager.cc reactor.cc
  zmq_utils.cc simple_rpc_channel rpc.cc
  ${PROTO_SOURCES})

add_executable(zsendrpc zsendrpc.cc)
target_link_libraries(zsendrpc zrpc zmq glog gflags ${PROTOBUF_LIBRARY})

# add_executable(client_pool client_pool.cc connection_manager.cc zmq_utils.cc)
# target_link_libraries(client_pool zmq glog gflags protobuf)
