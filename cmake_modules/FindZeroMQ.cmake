# - Try to find libzmq
# Once done, this will define
#
#  ZeroMQ_FOUND - system has libzmq
#  ZeroMQ_INCLUDE_DIRS - the libzmq include directories
#  ZeroMQ_LIBRARIES - link these to use libzmq
#  ZeroMQ_VERSION - Version of ZeroMQ found

include(LibFindMacros)

macro(ZMQ_VERSION_LOOKUP ZMQ_HEADER_PATH ZMQ_VERSION_MAJOR ZMQ_VERSION_MINOR ZMQ_VERSION_PATCH ZMQ_VERSION)
    # Version lookup using C header file
    if(EXISTS "${ZMQ_HEADER_PATH}/zmq.h")
      file(READ "${ZMQ_HEADER_PATH}/zmq.h" ZMQ_H_CONTENTS)
      string(REGEX MATCH "#define ZMQ_VERSION_MAJOR ([0-9]+)" _dummy "${ZMQ_H_CONTENTS}")
      set(${ZMQ_VERSION_MAJOR} "${CMAKE_MATCH_1}")
      string(REGEX MATCH "#define ZMQ_VERSION_MINOR ([0-9]+)" _dummy "${ZMQ_H_CONTENTS}")
      set(${ZMQ_VERSION_MINOR} "${CMAKE_MATCH_1}")
      string(REGEX MATCH "#define ZMQ_VERSION_PATCH ([0-9]+)" _dummy "${ZMQ_H_CONTENTS}")
      set(${ZMQ_VERSION_PATCH} "${CMAKE_MATCH_1}")
      set(${ZMQ_VERSION} "${${ZMQ_VERSION_MAJOR}}.${${ZMQ_VERSION_MINOR}}.${${ZMQ_VERSION_PATCH}}")
    endif()
endmacro(ZMQ_VERSION_LOOKUP)

if (NOT ZEROMQ_ROOT)
  set (ZEROMQ_ROOT $ENV{ZEROMQ_ROOT})
endif()

IF (UNIX)
	# Use pkg-config to get hints about paths
	libfind_pkg_check_modules(ZeroMQ_PKGCONF libzmq)

	# Include dir
	find_path(ZeroMQ_INCLUDE_DIR
	  NAMES zmq.hpp
	  PATHS ${ZEROMQ_ROOT}/include ${ZeroMQ_PKGCONF_INCLUDE_DIRS}
	)

	# Finally the library itself
	find_library(ZeroMQ_LIBRARY
	  NAMES zmq
	  PATHS ${ZEROMQ_ROOT}/lib ${ZeroMQ_PKGCONF_LIBRARY_DIRS}
	)

    ZMQ_VERSION_LOOKUP(${ZeroMQ_INCLUDE_DIR} ZMQ_VERSION_MAJOR ZMQ_VERSION_MINOR ZMQ_VERSION_PATCH ZeroMQ_VERSION)

ELSEIF (WIN32)

    # Find C header file
	find_path(ZeroMQ_INCLUDE_DIR
	  NAMES zmq.hpp
	  PATHS ${ZEROMQ_ROOT}/include ${CMAKE_INCLUDE_PATH}
	)

    ZMQ_VERSION_LOOKUP(${ZeroMQ_INCLUDE_DIR} ZMQ_VERSION_MAJOR ZMQ_VERSION_MINOR ZMQ_VERSION_PATCH ZeroMQ_VERSION)
    set(ZeroMQ_LIB_VERSION "${ZMQ_VERSION_MAJOR}_${ZMQ_VERSION_MINOR}_${ZMQ_VERSION_PATCH}")

    if(MSVC_VERSION MATCHES "1700")
      set(ZMQ_COMPILER_STRING "-v110")
    elseif(MSVC10)
      set(ZMQ_COMPILER_STRING "-v100")
    elseif(MSVC90)
      set(ZMQ_COMPILER_STRING "-v90")
    else()
      set(ZMQ_COMPILER_STRING "")
    endif()

    find_library(ZeroMQ_LIBRARY_RELEASE
      NAMES libzmq libzmq${ZMQ_COMPILER_STRING}-mt-${ZeroMQ_LIB_VERSION}
      PATHS ${ZEROMQ_ROOT}/lib ${CMAKE_LIB_PATH}
    )
    
    find_library(ZeroMQ_LIBRARY_DEBUG
      NAMES libzmq${ZMQ_COMPILER_STRING}-mt-gd-${ZeroMQ_LIB_VERSION}
      PATHS ${ZEROMQ_ROOT}/lib ${CMAKE_LIB_PATH}
    )
    
    if(ZeroMQ_LIBRARY_RELEASE AND ZeroMQ_LIBRARY_DEBUG)
        set(ZeroMQ_LIBRARY 
          optimized ${ZeroMQ_LIBRARY_RELEASE} 
          debug ${ZeroMQ_LIBRARY_DEBUG}
        )
    else()
        if(ZeroMQ_LIBRARY_RELEASE OR ZeroMQ_LIBRARY_DEBUG)
            set(ZeroMQ_LIBRARY ${ZeroMQ_LIBRARY_RELEASE} ${ZeroMQ_LIBRARY_DEBUG}
            )
        endif()    
    endif()

    
ENDIF()

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(ZeroMQ_PROCESS_INCLUDES ZeroMQ_INCLUDE_DIR ZeroMQ_INCLUDE_DIRS)
set(ZeroMQ_PROCESS_LIBS ZeroMQ_LIBRARY ZeroMQ_LIBRARIES)
libfind_process(ZeroMQ)
