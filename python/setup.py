import os
import compiler
import shutil
from distutils.core import Command
from distutils.command import build as build_module
from distutils.extension import Extension
from distutils.core import setup


def _build_zrpc_proto():
    compiler.generate_proto('../src/zrpc/proto/zrpc.proto', 'zrpc')


def _build_test_protos():
    compiler.generate_proto('../test/proto/search.proto', 'tests')
    compiler.generate_proto(
            '../test/proto/search.proto', 'tests',
            with_plugin='pyzrpc', suffix='_zrpc.py',
            plugin_binary='../build/src/zrpc/plugin/python/zrpc_python_plugin')


class build(build_module.build):
    def run(self):
        _build_zrpc_proto()
        _build_test_protos()
        shutil.copy('compiler.py', 'zrpc')
        build_module.build.run(self)


class gen_pyext(Command):
    user_options = []
    def initialize_options(self):
        pass
    def finalize_options(self):
        pass
    def run(self):
        os.system('cython --cplus cython/pywrapzrpc.pyx')


setup(
    name = "zrpc",
    version = "0.9",
    author = "Nadav Samet",
    author_email = "nadavs@google.com",
    description = "An RPC implementation for Protocol Buffer based on ZeroMQ",
    license = "BSD",
    keywords = "protocol-buffers rpc zeromq 0mq",
    packages=['zrpc', 'tests'],
    long_description='',
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Topic :: Software Development :: Libraries :: Python Modules"
        "License :: OSI Approved :: BSD License",
    ],
    cmdclass = {
        'build': build,
        'gen_pyext': gen_pyext,
    },
    ext_modules=[
        Extension("zrpc.pywrapzrpc", ["cython/pywrapzrpc.cpp"],
                  libraries=["zrpc", "protobuf", "glog", "zmq"],
                  include_dirs=['../include', '../build/src'],
                  library_dirs=['../build/deps/lib', '../build/src/zrpc'],
                  language='c++')
    ],
)
