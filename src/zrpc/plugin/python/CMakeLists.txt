find_package(ProtobufPlugin REQUIRED)
include_directories($PROTOBUF_INCLUDE_DIR)

set(ZRPC_SRCS zrpc_python_generator.cc zrpc_python_main.cc)
add_executable(protoc-gen-python_zrpc ${ZRPC_SRCS})
target_link_libraries(protoc-gen-python_zrpc
    ${PROTOBUF_LIBRARY} ${PROTOBUF_PROTOC_LIBRARY})
install(TARGETS protoc-gen-python_zrpc
    RUNTIME DESTINATION bin)
