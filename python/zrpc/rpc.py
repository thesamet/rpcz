import pywrapzrpc
from zrpc import zrpc_pb2

class RpcException(Exception):
  pass


class RpcApplicationError(RpcException):
  def __init__(self, application_error, message):
    self.application_error = application_error
    self.message = message

  def __repr__(self):
    return "RpcApplicationError(%r, %r)" % (self.application_error,
                                            self.message)

  def __str__(self):
    return "Error %d: %s" % (self.application_error,
                             self.message)


class RpcDeadlineExceeded(RpcException):
  pass


def RaiseRpcException(rpc):
  if rpc.status == zrpc_pb2.GenericRPCResponse.APPLICATION_ERROR:
    raise RpcApplicationError(rpc.application_error, rpc.error_message)
  else:
    if rpc.status == zrpc_pb2.GenericRPCResponse.DEADLINE_EXCEEDED:
      raise RpcDeadlineExceeded()

class RPC(pywrapzrpc.WrappedRPC):
  def wait(self):
    value = super(RPC, self).wait()
    if value == zrpc_pb2.GenericRPCResponse.TERMINATED:
      raise KeyboardInterrupt()
    if not self.ok():
      RaiseRpcException(self)
