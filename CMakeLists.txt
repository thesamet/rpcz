set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)
option(zrpc_build_tests "Build zrpc's tests." OFF)
option(zrpc_build_static "Build static library." OFF)
option(zrpc_build_examples "Build zrpc's examples." OFF)

include(build_deps)
BuildDependency(
    GFlags
    http://google-gflags.googlecode.com/files/gflags-1.7.tar.gz
    GFLAGS
    libgflags)

find_package(ProtobufPlugin REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread program_options)
find_package(ZeroMQ REQUIRED)

include_directories(${ZeroMQ_INCLUDE_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/src)

cmake_minimum_required (VERSION 2.8.0)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Werror -g -O1")
set(CMAKE_OSX_ARCHITECTURES x86_64)

add_subdirectory(src)

if(zrpc_build_tests)
  enable_testing()
  add_subdirectory(test)
endif(zrpc_build_tests)

if(zrpc_build_examples)
  add_subdirectory(examples/cpp)
endif(zrpc_build_examples)
