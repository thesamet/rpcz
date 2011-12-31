import pywraprpcz
from rpcz import rpcz_pb2


class RpcError(Exception):
  pass


class RpcApplicationError(RpcError):
  def __init__(self, application_error_code, message):
    self.application_error = application_error_code
    self.message = message

  def __repr__(self):
    return "RpcApplicationError(%r, %r)" % (self.application_error_code,
                                            self.message)

  def __str__(self):
    return "Error %d: %s" % (self.application_error_code,
                             self.message)


class RpcDeadlineExceeded(RpcError):
  pass


def RaiseRpcError(rpc):
  if rpc.status == rpcz_pb2.RpcResponseHeader.APPLICATION_ERROR:
    raise RpcApplicationError(rpc.application_error_code, rpc.error_message)
  elif rpc.status == rpcz_pb2.RpcResponseHeader.DEADLINE_EXCEEDED:
      raise RpcDeadlineExceeded()
  else:
    raise RpcError(rpc.status)

class RPC(pywraprpcz.WrappedRPC):
  def __init__(self, deadline_ms = None):
    super(RPC, self).__init__()
    if deadline_ms is not None:
      self.deadline_ms = deadline_ms

  def wait(self):
    super(RPC, self).wait()
    if not self.ok():
      RaiseRpcError(self)
