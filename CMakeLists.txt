set(CMAKE_CXX_COMPILER_WORKS:INTERNAL 1)
enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/src)

cmake_minimum_required (VERSION 2.8.6)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)
set(CMAKE_CXX_FLAGS "-Werror -Wall -g -O0")
set(CMAKE_OSX_ARCHITECTURES x86_64)

add_subdirectory(src)
add_subdirectory(test)
