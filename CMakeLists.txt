cmake_minimum_required (VERSION 2.8.10)

project(rpcz)

set(RPCZ_VERSION_MAJOR 0)
set(RPCZ_VERSION_MINOR 9)
set(RPCZ_VERSION_PATCH 0)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message("-- Build type: ${CMAKE_BUILD_TYPE}")

option(rpcz_build_tests "Build rpcz's tests." OFF)
option(rpcz_build_examples "Build rpcz's examples." OFF)
option(rpcz_enable_ipv6 "Enable IPv6 protocol." OFF)

if(MSVC)
    option(rpcz_build_static "Build static library." ON)
else()
    option(rpcz_build_static "Build static library." OFF)
endif()

find_package(ProtobufPlugin REQUIRED)

add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-DBOOST_ALL_DYN_LINK)
add_definitions(-DBOOST_LIB_DIAGNOSTIC)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost REQUIRED COMPONENTS thread program_options system date_time)

find_package(ZeroMQ REQUIRED)

include_directories(${ZeroMQ_INCLUDE_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/src)

if(MSVC)
    add_definitions("/W3 /wd4996")
    set(CMAKE_DEBUG_POSTFIX "_d")
else()
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -Werror")
endif()

set(CMAKE_OSX_ARCHITECTURES x86_64)

add_subdirectory(src)

if(rpcz_build_tests)
  enable_testing()
  add_subdirectory(test)
endif(rpcz_build_tests)

if(rpcz_build_examples)
  add_subdirectory(examples/cpp)
endif(rpcz_build_examples)

file(GLOB RPCZ_PUBLIC_HEADERS include/rpcz/*.hpp)
install(FILES ${RPCZ_PUBLIC_HEADERS} DESTINATION include/rpcz)
install(FILES ${PROJECT_BINARY_DIR}/src/rpcz/rpcz.pb.h DESTINATION include/rpcz)

###########################################################
# PACKAGE GENERATION
###########################################################

# Package target
set(CPACK_PACKAGE_NAME "rpcz")
set(CPACK_PACKAGE_VENDOR "Nadav Samet")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RPC implementation for Protocol Buffers over ZeroMQ")
set(CPACK_PACKAGE_VERSION_MAJOR "${RPCZ_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${RPCZ_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${RPCZ_VERSION_PATCH}")

if(UNIX)
    # Debian package specific config
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Philippe Hamelin")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libzmq3, libprotobuf8, protobuf-compiler, protobuf-c-compiler")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
endif()

if(CPACK_GENERATOR)
    include(CPack)
endif(CPACK_GENERATOR)

