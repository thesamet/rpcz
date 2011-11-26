project (ZRPC)

# The version number.
set (YOSH_STORE_VERSION_MAJOR 0)
set (YOSH_STORE_VERSION_MINOR 9)

add_subdirectory(proto)
add_subdirectory(plugin)

include_directories(${PROJECT_BINARY_DIR})

add_library(zrpc server.cc)
target_link_libraries(zrpc zrpc_pb)
