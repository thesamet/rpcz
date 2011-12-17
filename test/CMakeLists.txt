add_subdirectory(proto)
set(CTEST_OUTPUT_ON_FAILURE 1)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/test)

function(add_zrpc_test TESTNAME)
  CMAKE_PARSE_ARGUMENTS(OPTIONS "USE_GTEST_MAIN" "" "SRCS" ${ARGN})
  add_executable(${TESTNAME} ${OPTIONS_SRCS})
  set(LIBS zrpc zmq glog ${GTEST_LIBRARIES})
  if(OPTIONS_USE_GTEST_MAIN)
    list(APPEND LIBS ${GTEST_MAIN_LIBRARIES})
  endif(OPTIONS_USE_GTEST_MAIN)
  target_link_libraries(${TESTNAME} ${LIBS})
  add_test(${TESTNAME} ${TESTNAME} --logtostderr)
endfunction()

# add_executable(search_test search_main.cc)
# target_link_libraries(search_test
#     search_pb zrpc glog zmq gflags)
# 
# add_executable(client_test client_test.cc)
# target_link_libraries(client_test
#     search_pb zrpc glog zmq gflags)
# 

add_zrpc_test(callback_test USE_GTEST_MAIN SRCS callback_test.cc)
add_zrpc_test(event_manager_test SRCS event_manager_test.cc)
