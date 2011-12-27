add_subdirectory(proto)
set(CTEST_OUTPUT_ON_FAILURE 1)

include_directories(${PROJECT_BINARY_DIR}/test)
include_directories(${PROJECT_SOURCE_DIR}/src)
add_library(zrpc_gtest_main zrpc_gtest_main.cc)

include(build_deps)
BuildDependency(
    GLog
    http://google-glog.googlecode.com/files/glog-0.3.1-1.tar.gz
    GLOG
    libglog)

ExternalProject_Add(
    GTest
    URL http://googletest.googlecode.com/files/gtest-1.6.0.zip
    INSTALL_COMMAND ""
)
ExternalProject_Get_Property(GTest binary_dir)
ExternalProject_Get_Property(GTest source_dir)
include_directories(${source_dir}/include)    
link_directories(${binary_dir})

function(zrpc_test TESTNAME)
  CMAKE_PARSE_ARGUMENTS(OPTIONS "" "" "SRCS;LIBS" ${ARGN})
  add_executable(${TESTNAME} ${OPTIONS_SRCS})
  set(LIBS zrpc zrpc_gtest_main
      gtest ${GLOG_LIBRARIES}
  )
  list(APPEND LIBS ${OPTIONS_LIBS})
  target_link_libraries(${TESTNAME} ${LIBS})
  add_test(${TESTNAME} ${TESTNAME})
endfunction()

zrpc_test(function_server_test SRCS function_server_test.cc)
zrpc_test(callback_test SRCS callback_test.cc)
zrpc_test(connection_manager_test SRCS connection_manager_test.cc)
zrpc_test(event_manager_test SRCS event_manager_test.cc)
zrpc_test(client_server_test SRCS client_server_test.cc LIBS search_pb)
zrpc_test(application_test SRCS application_test.cc LIBS search_pb)
