add_subdirectory(proto)
set(CTEST_OUTPUT_ON_FAILURE 1)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/test)

add_executable(search_test search_main.cc)
target_link_libraries(search_test
    search_pb zrpc glog zmq gflags)

add_executable(client_test client_test.cc)
target_link_libraries(client_test
    search_pb zrpc glog zmq gflags)

add_executable(callback_test callback_test.cc)
target_link_libraries(callback_test ${GTEST_BOTH_LIBRARIES})

add_test(Callback callback_test --logtostderr)

# add_test(SearchTest search_test --logtostderr)
