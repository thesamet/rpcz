project (ZRPC)

# The version number.
set (ZRPC_VERSION_MAJOR 0)
set (ZRPC_VERSION_MINOR 9)

add_subdirectory(plugin)

protobuf_generate_cpp(ZRPC_PB_SRCS ZRPC_PB_HDRS proto/zrpc.proto)
set(PROTO_SOURCES ${ZRPC_PB_SRCS} ${ZRPC_PB_HDRS})

include_directories(${PROJECT_BINARY_DIR})
set(ZRPC_SOURCES clock.cc function_server.cc remote_response.cc
                 connection_manager.cc rpc_channel_impl.cc rpc.cc server.cc
                 reactor.cc zmq_utils.cc sync_event.cc event_manager.cc
                 ${PROTO_SOURCES})
add_library(zrpc ${ZRPC_SOURCES})
target_link_libraries(zrpc ${ZeroMQ_LIBRARIES}
                      ${GFLAGS_LIBRARIES} ${GLOG_LIBRARIES} ${PROTOBUF_LIBRARY}
                      ${Boost_THREAD_LIBRARIES})

add_executable(zsendrpc zsendrpc.cc)
target_link_libraries(zsendrpc zrpc)
