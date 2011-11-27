#!/usr/bin/env python

class GeneratedServiceType(type):
    def __new__(cls, name, bases, attrs):
        return super(GeneratedServiceType, cls).__new__(cls, name, bases,
                                                        attrs)


def CallMethod(stub, rpc, request, response, callback, method, **kwargs):
    print stub, method, rpc, request, response, callback, kwargs


def _BuildStubMethod(method_descriptor):
    def call(stub, request, rpc=None):
        return stub._connection.CallMethod(stub.DESCRIPTOR.full_name,
                                           method_descriptor,
                                           request, rpc=rpc)
    return call


def _StubInitMethod(stub, connection):
    stub._connection = connection


class GeneratedServiceStubType(GeneratedServiceType):
    def __new__(cls, name, bases, attrs):
        descriptor = attrs['DESCRIPTOR']
        attrs['__init__'] = _StubInitMethod
        for method in descriptor.methods:
            attrs[method.name] = _BuildStubMethod(method)

        return super(GeneratedServiceStubType, cls).__new__(cls, name, bases,
                                                            attrs)
