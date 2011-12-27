set(ZRPC_PLUGIN_ROOT ${CMAKE_CURRENT_LIST_DIR}/../build/src/zrpc/plugin)

function(PROTOBUF_GENERATE_ZRPC SRCS HDRS)
  set(PLUGIN_BIN ${ZRPC_PLUGIN_ROOT}/cpp/protoc-gen-cpp_zrpc)
  PROTOBUF_GENERATE_MULTI(PLUGIN "cpp_zrpc" PROTOS ${ARGN}
                          OUTPUT_STRUCT "_SRCS:.zrpc.cc;_HDRS:.zrpc.h"
                          FLAGS "--plugin=protoc-gen-cpp_zrpc=${PLUGIN_BIN}"
                          DEPENDS ${PLUGIN_BIN})
  set(${SRCS} ${_SRCS} PARENT_SCOPE)
  set(${HDRS} ${_HDRS} PARENT_SCOPE)
endfunction()

function(PROTOBUF_GENERATE_PYTHON_ZRPC SRCS)
  set(PLUGIN_BIN ${ZRPC_PLUGIN_ROOT}/python/protoc-gen-python_zrpc)
  PROTOBUF_GENERATE_MULTI(PLUGIN "python_zrpc" PROTOS ${ARGN}
                          OUTPUT_STRUCT "_SRCS:_zrpc.py"
                          FLAGS "--plugin=protoc-gen-python_zrpc=${PLUGIN_BIN}"
                          DEPENDS ${PLUGIN_BIN})
  set(${SRCS} ${_SRCS} PARENT_SCOPE)
endfunction()
