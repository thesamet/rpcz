add_subdirectory(proto)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/test)

#add_executable(search_test search_main.cc)
# target_link_libraries(search_test
#   search_pb zrpc glog zmq gflags)

#add_test(SearchTest search_test --logtostderr)
