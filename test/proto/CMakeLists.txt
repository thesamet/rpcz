find_package(Protobuf REQUIRED)

protobuf_generate_cpp(SEARCH_PB_SRCS SEARCH_PB_HDRS search.proto)

add_library(search_pb ${SEARCH_PB_SRCS} ${SEARCH_PB_HDRS})
target_link_libraries(search_pb ${PROTOBUF_LIBRARY} zrpc_pb)
