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


class BlockingConnection(object):
    def __init__(self, socket):
        self._socket = socket

    def CallMethod(self, service_name, method_descriptor, request, rpc=None):
        # TODO: use RPC object, copy RPC from response
        generic_request = zrpc_pb2.GenericRPCRequest()
        generic_request.service = service_name
        generic_request.method = method_descriptor.name
        generic_request.payload = request.SerializeToString()
        self._socket.send(generic_request.SerializeToString())
        generic_response = zrpc_pb2.GenericRPCResponse()
        resp = self._socket.recv()
        generic_response.ParseFromString(resp)
        if generic_response.status == generic_response.OK:
            # happy path
            response = method_descriptor.output_type._concrete_class()
            response.ParseFromString(generic_response.payload)
            return response
        else:
            self._HandleError(generic_response)


    def _HandleError(self, generic_response):
        # raise an exception
        if generic_response.status == generic_response.APPLICATION_ERROR:
            rpc = RPC()
            rpc.status = generic_response.status
            rpc.error = generic_response.error
            rpc.application_error = generic_response.application_error
            raise ApplicalicationError(rpc)
