set(CMAKE_CXX_COMPILER_WORKS:INTERNAL 1)
enable_testing()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)
option(zrpc_build_tests "Build zrpc's tests." OFF)

find_package(ProtobufPlugin REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread)
include_directories(${PROTOBUF_INCLUDE_DIR})

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/src)

cmake_minimum_required (VERSION 2.8.6)
set(CMAKE_CXX_FLAGS "-Werror -O3")
set(CMAKE_OSX_ARCHITECTURES x86_64)

add_subdirectory(src)
if(zrpc_build_tests)
  add_subdirectory(test)
endif(zrpc_build_tests)
