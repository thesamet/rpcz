set(CMAKE_CXX_COMPILER_WORKS:INTERNAL 1)
enable_testing()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)

find_package(ProtobufPlugin REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/src)

cmake_minimum_required (VERSION 2.8.6)
set(CMAKE_CXX_FLAGS "-Werror -O3")
set(CMAKE_OSX_ARCHITECTURES x86_64)

add_subdirectory(src)
add_subdirectory(test)
