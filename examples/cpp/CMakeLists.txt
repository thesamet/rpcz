include(zrpc_functions)
find_package(ProtobufPlugin REQUIRED)
PROTOBUF_GENERATE_CPP(SEARCH_PB_SRCS SEARCH_PB_HDRS ../common/search.proto)
PROTOBUF_GENERATE_ZRPC(SEARCH_ZRPC_SRCS SEARCH_ZRPC_HDRS ../common/search.proto)

add_library(example_pb ${SEARCH_PB_SRCS} ${SEARCH_PB_HDRS} ${SEARCH_ZRPC_SRCS}
                      ${SEARCH_ZRPC_HDRS})
target_link_libraries(example_pb ${PROTOBUF_LIBRARY})
include_directories(${PROJECT_BINARY_DIR}/examples)

add_executable(example_client example_client.cc)
target_link_libraries(example_client zrpc example_pb)

add_executable(example_server example_server.cc)
target_link_libraries(example_server zrpc example_pb)
