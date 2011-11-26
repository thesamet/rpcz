enable_testing()

cmake_minimum_required (VERSION 2.8.6)
set(CMAKE_CXX_FLAGS "-Werror")
include_directories("src")

add_subdirectory(src)
add_subdirectory(test)
