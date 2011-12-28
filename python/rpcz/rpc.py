import pywraprpcz
from rpcz import rpcz_pb2


class RpcError(Exception):
  pass


class RpcApplicationError(RpcError):
  def __init__(self, application_error, message):
    self.application_error = application_error
    self.message = message

  def __repr__(self):
    return "RpcApplicationError(%r, %r)" % (self.application_error,
                                            self.message)

  def __str__(self):
    return "Error %d: %s" % (self.application_error,
                             self.message)


class RpcDeadlineExceeded(RpcError):
  pass


def RaiseRpcError(rpc):
  if rpc.status == rpcz_pb2.RpcResponseHeader.APPLICATION_ERROR:
    raise RpcApplicationError(rpc.application_error, rpc.error_message)
  else:
    if rpc.status == rpcz_pb2.RpcResponseHeader.DEADLINE_EXCEEDED:
      raise RpcDeadlineExceeded()


class RPC(pywraprpcz.WrappedRPC):
  def wait(self):
    super(RPC, self).wait()
    if not self.ok():
      RaiseRpcError(self)
