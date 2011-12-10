#!/usr/bin/env python

class GeneratedServiceType(type):
    def __new__(cls, name, bases, attrs):
        return super(GeneratedServiceType, cls).__new__(cls, name, bases,
                                                        attrs)

def _BuildStubMethod(method_descriptor):
    def call(stub, rpc, request, response, callback):
        return stub._channel.CallMethod(stub.DESCRIPTOR.name,
                                        method_descriptor.name,
                                        rpc, request, response, callback)
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
