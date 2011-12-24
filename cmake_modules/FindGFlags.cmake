# - Try to find gflags
# Once done, this will define
#
#  GFlags_FOUND - system has gflags
#  GFlags_INCLUDE_DIRS - the gflags include directories
#  GFlags_LIBRARIES - link these to use gflags

include(LibFindMacros)

IF (UNIX)
	# Use pkg-config to get hints about paths
	libfind_pkg_check_modules(GFlags_PKGCONF libgflags)

	# Include dir
	find_path(GFlags_INCLUDE_DIR
	  NAMES gflags/gflags.h
	  PATHS ${GFLAGS_ROOT}/include ${GFlags_PKGCONF_INCLUDE_DIRS}
	)

	# Finally the library itself
	find_library(GFlags_LIBRARY
	  NAMES gflags
	  PATHS ${GFLAGS_ROOT}/lib ${GFlags_PKGCONF_LIBRARY_DIRS}
	)
ELSEIF (WIN32)
	find_path(GFlags_INCLUDE_DIR
	  NAMES gflags/gflags.h
	  PATHS ${GFLAGS_ROOT}/include ${CMAKE_INCLUDE_PATH}
	)
	# Finally the library itself
	find_library(GFlags_LIBRARY
	  NAMES gflags
	  PATHS ${GFLAGS_ROOT}/lib ${CMAKE_LIB_PATH}
	)
ENDIF()

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GFlags_PROCESS_INCLUDES GFlags_INCLUDE_DIR GFlags_INCLUDE_DIRS)
set(GFlags_PROCESS_LIBS GFlags_LIBRARY GFlags_LIBRARIES)
libfind_process(GFlags)
