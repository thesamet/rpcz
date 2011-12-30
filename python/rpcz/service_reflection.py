#!/usr/bin/env python
import rpcz.rpc

class GeneratedServiceType(type):
    def __new__(cls, name, bases, attrs):
        return super(GeneratedServiceType, cls).__new__(cls, name, bases,
                                                        attrs)


def _BuildStubMethod(method_descriptor):
    def call(stub, request, rpc=None, callback=None,
             deadline_ms=None):
        response = method_descriptor.output_type._concrete_class()
        if rpc is None:
            blocking_mode = True
            rpc = rpcz.rpc.RPC(deadline_ms = deadline_ms)
        else:
            blocking_mode = False
            if deadline_ms is not None:
              raise ValueError("'rpc' and 'deadline_ms' cannot be both "
                               "specified. Use rpc.deadline_ms to set a "
                               "deadline")
        stub._channel.CallMethod(stub.DESCRIPTOR.name,
                                 method_descriptor.name,
                                 request, response, rpc, callback)
        if blocking_mode:
            rpc.wait()
            return response
    return call


def _StubInitMethod(stub, channel):
    stub._channel = channel


class GeneratedServiceStubType(GeneratedServiceType):
    def __new__(cls, name, bases, attrs):
        descriptor = attrs['DESCRIPTOR']
        attrs['__init__'] = _StubInitMethod
        for method in descriptor.methods:
            attrs[method.name] = _BuildStubMethod(method)

        return super(GeneratedServiceStubType, cls).__new__(cls, name, bases,
                                                            attrs)
