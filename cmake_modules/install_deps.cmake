include(${CMAKE_ROOT}/Modules/CMakeParseArguments.cmake)
include(library_suffix)
include(ExternalProject)

function(BuildDependency TARGETNAME URL VARNAME LIBNAME)
    CMAKE_PARSE_ARGUMENTS(OPTIONS "" "CONFIGURE_ARGS"
        "" ${ARGN})
    ExternalProject_Add(
        ${TARGETNAME}
        URL ${URL}
        INSTALL_DIR ${PROJECT_BINARY_DIR}/deps
        CONFIGURE_COMMAND
            <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
                                   ${OPTIONS_CONFIGURE_ARGS}
    )
    ExternalProject_Get_Property(${TARGETNAME} install_dir)
    include_directories(${install_dir}/include)
    set(${VARNAME}_LIBRARIES ${install_dir}/lib/${LIBNAME}.${link_library_suffix}
      PARENT_SCOPE)
endfunction()

BuildDependency(
    GFlags
    http://google-gflags.googlecode.com/files/gflags-1.7.tar.gz
    GFLAGS
    libgflags)

BuildDependency(
    GLog
    http://google-glog.googlecode.com/files/glog-0.3.1-1.tar.gz
    GLOG
    libglog
    CONFIGURE_ARGS --with-gflags=${PROJECT_BINARY_DIR}/deps)

BuildDependency(
    ZeroMQ
    http://download.zeromq.org/zeromq-2.1.11.tar.gz
    ZEROMQ
    libzmq)
