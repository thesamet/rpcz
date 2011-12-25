set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)
option(zrpc_build_tests "Build zrpc's tests." OFF)
option(zrpc_build_static "Build static library." OFF)

include(library_suffix)
include(ExternalProject)
# Install GFlags
ExternalProject_Add(
    GFlagsLib
    URL http://google-gflags.googlecode.com/files/gflags-1.7.tar.gz
    CONFIGURE_COMMAND <SOURCE_DIR>/configure  --prefix=<INSTALL_DIR>
    )
ExternalProject_Get_Property(GFlagsLib install_dir)
include_directories(${install_dir}/include)
set(GFLAGS_LIBRARIES ${install_dir}/lib/libgflags.${link_library_suffix})
set(GFLAGS_PREFIX ${install_dir})

# Install GLog
ExternalProject_Add(
    GLogLib
    URL http://google-glog.googlecode.com/files/glog-0.3.1-1.tar.gz
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-gflags=${GFLAGS_PREFIX}
    )
ExternalProject_Get_Property(GLogLib install_dir)
include_directories(${install_dir}/include)
set(GLOG_LIBRARIES ${install_dir}/lib/libglog.${link_library_suffix})

find_package(ZeroMQ REQUIRED)
find_package(ProtobufPlugin REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread)
include_directories(${ZeroMQ_INCLUDE_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/src)

cmake_minimum_required (VERSION 2.8.0)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-sign-compare -Werror -g -O1")
set(CMAKE_OSX_ARCHITECTURES x86_64)

add_subdirectory(src)
if(zrpc_build_tests)
  enable_testing()
  add_subdirectory(test)
endif(zrpc_build_tests)
