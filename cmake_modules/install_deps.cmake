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

# Install ZeroMQ
ExternalProject_Add(
    ZeroMQ
    URL http://download.zeromq.org/zeromq-2.1.11.tar.gz
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-gflags=${GFLAGS_PREFIX}
    )
ExternalProject_Get_Property(ZeroMQ install_dir)
include_directories(${install_dir}/include)
set(ZEROMQ_LIBRARIES ${install_dir}/lib/libzmq.${link_library_suffix})

