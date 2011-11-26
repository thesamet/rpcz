find_package(ProtobufPlugin REQUIRED)

function(PROTOBUF_GENERATE_ZRPC SRCS HDRS)
  PROTOBUF_GENERATE_MULTI(PLUGIN "zrpc" PROTOS ${ARGN}
                          OUTPUT_STRUCT "_SRCS:.zrpc.cc;_HDRS:.zrpc.h"
                          FLAGS "--plugin=protoc-gen-zrpc=${PROJECT_BINARY_DIR}/src/zrpc/plugin/cpp/zrpc_cpp_plugin"
                          DEPENDS ${PROJECT_BINARY_DIR}/src/zrpc/plugin/cpp/zrpc_cpp_plugin)
  set(${SRCS} ${_SRCS} PARENT_SCOPE)
  set(${HDRS} ${_HDRS} PARENT_SCOPE)
endfunction()

# PROTOBUF_GENERATE_CPP(SEARCH_PB_SRCS SEARCH_PB_HDRS search.proto)
PROTOBUF_GENERATE_ZRPC(SEARCH_ZRPC_SRCS SEARCH_ZRPC_HDRS search.proto)

add_library(search_pb ${SEARCH_PB_SRCS} ${SEARCH_PB_HDRS} ${SEARCH_ZRPC_SRCS}
                      ${SEARCH_ZRPC_HDRS})
target_link_libraries(search_pb ${PROTOBUF_LIBRARY} zrpc_pb)
