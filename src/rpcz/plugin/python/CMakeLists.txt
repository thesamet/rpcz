find_package(ProtobufPlugin REQUIRED)
include_directories($PROTOBUF_INCLUDE_DIR)

set(RPCZ_SRCS rpcz_python_generator.cc rpcz_python_main.cc)
add_executable(protoc-gen-python_rpcz ${RPCZ_SRCS})
target_link_libraries(protoc-gen-python_rpcz
    ${PROTOBUF_LIBRARIES} ${PROTOBUF_PROTOC_LIBRARIES})
install(TARGETS protoc-gen-python_rpcz
    RUNTIME DESTINATION bin)
