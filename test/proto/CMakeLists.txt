include(zrpc_functions)
find_package(ProtobufPlugin REQUIRED)

PROTOBUF_GENERATE_PYTHON(SEARCH_PB_PY_SRCS search.proto)
PROTOBUF_GENERATE_CPP(SEARCH_PB_SRCS SEARCH_PB_HDRS search.proto)

PROTOBUF_GENERATE_PYTHON_ZRPC(SEARCH_PB_PYZRPC_SRCS search.proto)
PROTOBUF_GENERATE_ZRPC(SEARCH_ZRPC_SRCS SEARCH_ZRPC_HDRS search.proto)

add_library(search_pb ${SEARCH_PB_SRCS} ${SEARCH_PB_HDRS} ${SEARCH_ZRPC_SRCS}
                      ${SEARCH_ZRPC_HDRS})
target_link_libraries(search_pb ${PROTOBUF_LIBRARY})

add_custom_target(_force_python_protos ALL DEPENDS ${SEARCH_PB_PY_SRCS}
    ${SEARCH_PB_PYZRPC_SRCS})
