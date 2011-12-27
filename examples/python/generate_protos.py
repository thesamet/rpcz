#!/usr/bin/env python

from zrpc import compiler

compiler.generate_proto('../common/search.proto', '.')
compiler.generate_proto(
        '../common/search.proto', '.',
        with_plugin='python_zrpc', suffix='_zrpc.py',
        plugin_binary=
            '../../build/src/zrpc/plugin/python/protoc-gen-python_zrpc')
