import os
import sys

from distutils.core import Command
from distutils.command import build as build_module
from setuptools.command import develop as develop_module
from distutils import spawn
from setuptools import setup


def _generate_proto(source, output_dir):
    """Invokes the Protocol Compiler to generate a _pb2.py from the given
    .proto file.  Does nothing if the output already exists and is newer than
    the input."""
    protoc = spawn.find_executable("protoc")
    if protoc is None:
        sys.stderr.write("protoc not found. Make sure that it is in the path.")
        sys.exit(-1)

    output = os.path.join(
            output_dir,
            os.path.basename(source.replace(".proto", "_pb2.py")))

    if not os.path.exists(source):
        print "Can't find required file: " + source
        sys.exit(-1)

    if (os.path.exists(output) and
        os.path.getmtime(source) <= os.path.getmtime(output)):
        print "Generated proto %s is up-to-date." % output
        return

    print "Generating %s" % output

    protoc_command = protoc + ' -I "%s" --python_out="%s" "%s"' % (
            os.path.dirname(source), output_dir, source)
    if os.system(protoc_command) != 0:
        print "Error running protoc."
        sys.exit(-1)


def _build_my_proto():
    _generate_proto('../src/zrpc/proto/zrpc.proto', 'zrpc')


class build(build_module.build):
    def run(self):
        _build_my_proto()
        build_module.build.run(self)


class develop(develop_module.develop):
    def run(self):
        _build_my_proto()
        develop_module.develop.run(self)

    
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
        'develop': develop,
    }
)
