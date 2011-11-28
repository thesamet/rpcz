add_subdirectory(proto)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/test)

add_executable(search_test search_main.cc)
find_library(zmq_valgrind NAMES zmq PATHS /tmp/zeromq-2.1.10/src/.libs NO_DEFAULT_PATH)
message("zmqlib -- ${zmq_valgrind}")
target_link_libraries(search_test
    search_pb zrpc glog ${zmq_valgrind} gflags)

# add_test(SearchTest search_test --logtostderr)
