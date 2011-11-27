#!/usr/bin/env python

class GeneratedServiceType(type):
    def __new__(cls, name, bases, attrs):
        descriptor = attrs.pop('DESCRIPTOR')
        return super(GeneratedServiceType, cls).__new__(cls, name, bases,
                                                        uppercase_attr)

class GeneratedServiceStubType(type):
    def __new__(cls, name, bases, attrs):
        return super(GeneratedServiceStubType, cls).__new__(cls, name, bases,
                                                            attrs)
