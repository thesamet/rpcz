project (ZRPC)

# The version number.
set (ZRPC_VERSION_MAJOR 0)
set (ZRPC_VERSION_MINOR 9)

add_subdirectory(plugin)

protobuf_generate_cpp(ZRPC_PB_SRCS ZRPC_PB_HDRS proto/zrpc.proto)
set(PROTO_SOURCES ${ZRPC_PB_SRCS} ${ZRPC_PB_HDRS})

include_directories(${PROJECT_BINARY_DIR})
set(ZRPC_SOURCES
    application.cc clock.cc connection_manager.cc event_manager.cc
    function_server.cc reactor.cc remote_response.cc rpc.cc
    rpc_channel_impl.cc server.cc sync_event.cc zmq_utils.cc
    ${PROTO_SOURCES})
set(ZRPC_LIB_DEPS ${ZeroMQ_LIBRARIES}
                  ${PROTOBUF_LIBRARY}
                  ${Boost_THREAD_LIBRARIES})

add_library(zrpc SHARED ${ZRPC_SOURCES})
target_link_libraries(zrpc ${ZRPC_LIB_DEPS})
set(ZRPC_TARGET_LIBS zrpc)

if(zrpc_build_static)
    add_library(zrpc_static STATIC ${ZRPC_SOURCES})
    target_link_libraries(zrpc_static ${ZRPC_LIB_DEPS})
    set_target_properties(zrpc_static PROPERTIES OUTPUT_NAME zrpc)
    list(APPEND ZRPC_TARGET_LIBS zrpc_static)
endif(zrpc_build_static)

add_executable(zsendrpc zsendrpc.cc)
target_link_libraries(zsendrpc zrpc ${Boost_PROGRAM_OPTIONS_LIBRARIES})

install(TARGETS ${ZRPC_TARGET_LIBS} zsendrpc
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
