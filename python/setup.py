from setuptools import setup

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
)
