add_subdirectory(proto)
set(CTEST_OUTPUT_ON_FAILURE 1)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/test)

add_library(zrpc_gtest_main zrpc_gtest_main.cc)

function(add_zrpc_test TESTNAME)
  CMAKE_PARSE_ARGUMENTS(OPTIONS "" "" "SRCS;LIBS" ${ARGN})
  add_executable(${TESTNAME} ${OPTIONS_SRCS})
  set(LIBS zrpc zmq glog gflags zrpc_gtest_main
    ${GTEST_LIBRARIES}
    ${PROTOBUF_LIBRARY}
    ${Boost_THREAD_LIBRARIES})
  list(APPEND LIBS ${OPTIONS_LIBS})
  target_link_libraries(${TESTNAME} ${LIBS})
  add_test(${TESTNAME} ${TESTNAME} --logtostderr)
  message(${TESTNAME} ${LIBS})
endfunction()

# add_executable(client_test client_test.cc)
# target_link_libraries(client_test
#     search_pb zrpc glog zmq gflags)


add_zrpc_test(callback_test SRCS callback_test.cc)
add_zrpc_test(event_manager_test SRCS event_manager_test.cc)
add_zrpc_test(connection_manager_test SRCS connection_manager_test.cc)
add_zrpc_test(rpc_test SRCS search_main.cc LIBS search_pb)
