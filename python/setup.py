import os
import sys

from distutils.core import Command
from distutils.command import build as build_module
from distutils.extension import Extension
from distutils.core import setup
from distutils import spawn


def _generate_proto(source, output_dir,
                    with_plugin='python', suffix='_pb2.py', plugin_binary=None):
    """Invokes the Protocol Compiler to generate a _pb2.py from the given
    .proto file.  Does nothing if the output already exists and is newer than
    the input."""
    protoc = spawn.find_executable("protoc")
    if protoc is None:
        sys.stderr.write("protoc not found. Make sure that it is in the path.")
        sys.exit(-1)

    output = os.path.join(
            output_dir,
            os.path.basename(source.replace(".proto", suffix)))

    if not os.path.exists(source):
        print "Can't find required file: " + source
        sys.exit(-1)

    if (os.path.exists(output) and
        os.path.getmtime(source) <= os.path.getmtime(output)):
        print "Generated proto %s is up-to-date." % output
        return

    print "Generating %s" % output

    protoc_command = protoc + ' -I "%s" --%s_out="%s" "%s"' % (
            os.path.dirname(source), with_plugin, output_dir, source)
    if plugin_binary:
        protoc_command += ' --plugin=protoc-gen-%s=%s' % (with_plugin, plugin_binary)

    if os.system(protoc_command) != 0:
        print "Error running protoc."
        sys.exit(-1)


def _build_zrpc_proto():
    _generate_proto('../src/zrpc/proto/zrpc.proto', 'zrpc')


def _build_test_protos():
    _generate_proto('../test/proto/search.proto', 'tests')
    _generate_proto(
            '../test/proto/search.proto', 'tests',
            with_plugin='pyzrpc', suffix='_zrpc.py',
            plugin_binary='../build/src/zrpc/plugin/python/zrpc_python_plugin')


class build(build_module.build):
    def run(self):
        _build_zrpc_proto()
        _build_test_protos()
        build_module.build.run(self)


setup(
    name = "zrpc",
    version = "0.9",
    author = "Nadav Samet",
    author_email = "nadavs@google.com",
    description = "An RPC implementation for Protocol Buffer based on ZeroMQ",
    install_requires = ['protobuf >= 2.4'],
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
    },
    ext_modules=[
        Extension("pywrapzrpc", ["pywrapzrpc.cpp"], libraries=["zrpc",
                                                               "protobuf",
                                                               "glog",
                                                               "zmq"])
    ],
)
