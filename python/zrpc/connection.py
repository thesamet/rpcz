#!/usr/bin/env python
from zrpc import zrpc_pb2


class RPCException(Exception):
    pass


class ApplicalicationError(RPCException):
    def __init__(self, rpc):
        self.rpc = rpc

    def __str__(self):
        if not self.rpc.error:
            return 'Application error %d' % self.rpc.application_error
        else:
            return 'Application error %d: "%s"' % (self.rpc.application_error,
                                                   self.rpc.error)


class RPC(object):
    def __init__(self):
        pass
