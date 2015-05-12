find_package(ProtobufPlugin REQUIRED)
include_directories($PROTOBUF_INCLUDE_DIR)

set(RPCZ_SRCS rpcz_cpp_generator.cc file_generator.cc rpcz_cpp_main.cc
              rpcz_cpp_service.cc)
add_executable(protoc-gen-cpp_rpcz ${RPCZ_SRCS})
target_link_libraries(protoc-gen-cpp_rpcz ${PROTOBUF_LIBRARIES} ${PROTOBUF_PROTOC_LIBRARIES})
install(TARGETS protoc-gen-cpp_rpcz
    RUNTIME DESTINATION bin)
