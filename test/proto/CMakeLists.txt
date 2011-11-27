find_package(ProtobufPlugin REQUIRED)

function(PROTOBUF_GENERATE_ZRPC SRCS HDRS)
  PROTOBUF_GENERATE_MULTI(PLUGIN "zrpc" PROTOS ${ARGN}
                          OUTPUT_STRUCT "_SRCS:.zrpc.cc;_HDRS:.zrpc.h"
                          FLAGS "--plugin=protoc-gen-zrpc=${PROJECT_BINARY_DIR}/src/zrpc/plugin/cpp/zrpc_cpp_plugin"
                          DEPENDS ${PROJECT_BINARY_DIR}/src/zrpc/plugin/cpp/zrpc_cpp_plugin)
  set(${SRCS} ${_SRCS} PARENT_SCOPE)
  set(${HDRS} ${_HDRS} PARENT_SCOPE)
endfunction()

function(PROTOBUF_GENERATE_PYTHON_ZRPC SRCS)
  PROTOBUF_GENERATE_MULTI(PLUGIN "pyzrpc" PROTOS ${ARGN}
                          OUTPUT_STRUCT "_SRCS:_zrpc.py"
                          FLAGS "--plugin=protoc-gen-pyzrpc=${PROJECT_BINARY_DIR}/src/zrpc/plugin/python/zrpc_python_plugin"
                          DEPENDS ${PROJECT_BINARY_DIR}/src/zrpc/plugin/python/zrpc_python_plugin)
  set(${SRCS} ${_SRCS} PARENT_SCOPE)
endfunction()

PROTOBUF_GENERATE_PYTHON(SEARCH_PB_PY_SRCS search.proto)
PROTOBUF_GENERATE_CPP(SEARCH_PB_SRCS SEARCH_PB_HDRS search.proto)

PROTOBUF_GENERATE_PYTHON_ZRPC(SEARCH_PB_PYZRPC_SRCS search.proto)
PROTOBUF_GENERATE_ZRPC(SEARCH_ZRPC_SRCS SEARCH_ZRPC_HDRS search.proto)

add_library(search_pb ${SEARCH_PB_SRCS} ${SEARCH_PB_HDRS} ${SEARCH_ZRPC_SRCS}
                      ${SEARCH_ZRPC_HDRS})
target_link_libraries(search_pb ${PROTOBUF_LIBRARY} zrpc_pb)

message("hello ${SEARCH_PB_PY_SRCS}")
add_custom_target(_force_python_protos ALL DEPENDS ${SEARCH_PB_PY_SRCS}
    ${SEARCH_PB_PYZRPC_SRCS})
