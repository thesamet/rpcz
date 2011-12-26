from cpython cimport Py_DECREF, Py_INCREF
from libc.stdlib cimport malloc, free


# cdef extern from "glog/logging.h" namespace "google":
#     cdef void InstallFailureSignalHandler()
#     cdef void InitGoogleLogging(char*)


cdef extern from "Python.h":
    ctypedef int PyGILState_STATE
    PyGILState_STATE    PyGILState_Ensure               ()   nogil
    void                PyGILState_Release              (PyGILState_STATE) nogil
    void PyEval_InitThreads()

PyEval_InitThreads()

cdef extern from "zrpc/connection_manager.h" namespace "zrpc":
    cdef void InstallSignalHandler()

cdef extern from "zrpc/callback.h" namespace "zrpc":
  pass

def Init():
    import sys
    # InstallFailureSignalHandler()
    # InitGoogleLogging(sys.argv[0])
    InstallSignalHandler()


cdef extern from "string" namespace "std":
    cdef cppclass string:
        string()
        string(char*)
        string(char*, size_t)
        size_t size()
        char* c_str()


cdef string make_string(pystring) except *:
    return string(pystring, len(pystring))


cdef ptr_string_to_pystring(string* s):
    return s.c_str()[:s.size()]


cdef string_to_pystring(string s):
    return s.c_str()[:s.size()]


# cdef extern from "zrpc/rpc.h" namespace "zrpc":
#     cdef enum Status "zrpc::GeneratedRPCResponse::Status":
#         APPLICATION_ERROR, OK, DEADLINE_EXCEEDED


cdef extern from "zrpc/rpc.h" namespace "zrpc":
    cdef cppclass _RPC "zrpc::RPC":
        bint OK()
        int GetStatus()
        string GetErrorMessage()
        int GetApplicationError()
        int GetDeadlineMs()
        void SetDeadlineMs(int)
        int Wait() nogil


cdef class WrappedRPC:
    cdef _RPC *thisptr
    def __cinit__(self):
        self.thisptr = new _RPC()
    def __dealloc__(self):
        del self.thisptr
    def ok(self):
        return self.thisptr.OK()
    def wait(self):
        with nogil:
            value = self.thisptr.Wait()
        if value == -1:
          raise KeyboardInterrupt()

    property status:
        def __get__(self):
            return self.thisptr.GetStatus()
    property application_error:
        def __get__(self):
            return self.thisptr.GetApplicationError()
    property error_message:
        def __get__(self):
            return string_to_pystring(self.thisptr.GetErrorMessage())
    property deadline_ms:
        def __get__(self):
            return self.thisptr.GetDeadlineMs()
        def __set__(self, value):
            self.thisptr.SetDeadlineMs(value)


cdef struct ClosureWrapper:
    string* response_str
    void* response_obj
    void* callback


cdef extern from "zrpc/macros.h" namespace "zrpc":
    cdef cppclass Closure:
        pass

    Closure* NewCallback(void(ClosureWrapper*) nogil, ClosureWrapper*)

cdef void PythonCallbackBridge(ClosureWrapper *closure_wrapper) with gil:
    # We don't really have the GIL when this function is called, and it is
    # ran from a non-python thread.
    (<object>closure_wrapper.response_obj).ParseFromString(
            ptr_string_to_pystring(closure_wrapper.response_str))
    callback = <object>closure_wrapper.callback
    if callback is not None:
      callback()
    Py_DECREF(<object>closure_wrapper.response_obj)
    Py_DECREF(<object>closure_wrapper.callback)
    del closure_wrapper.response_str
    free(closure_wrapper)


cdef extern from "zrpc/rpc_channel.h" namespace "zrpc":
    cdef cppclass _RpcChannel "zrpc::RpcChannel":
        void CallMethod0(string service_name, string method_name,
                         _RPC* rpc, string request, string* response,
                         Closure* callback) except +


cdef class RpcChannel:
    cdef _RpcChannel *thisptr
    def __dealloc__(self):
        del self.thisptr
    def __init__(self):
        raise TypeError("Use Application.CreateRpcChannel to create a "
                        "RpcChannel.")
    def CallMethod(self, service_name, method_name,
                   WrappedRPC rpc, request, response, callback):
        cdef ClosureWrapper* closure_wrapper = <ClosureWrapper*>malloc(
                sizeof(ClosureWrapper))
        closure_wrapper.response_str = new string()
        closure_wrapper.response_obj = <void*>response
        closure_wrapper.callback = <void*>callback
        Py_INCREF(response)
        Py_INCREF(callback)
        self.thisptr.CallMethod0(
                make_string(service_name),
                make_string(method_name),
                rpc.thisptr,
                make_string(request.SerializeToString()),
                closure_wrapper.response_str,
                NewCallback(
                    PythonCallbackBridge, closure_wrapper))


cdef extern from "zrpc/zrpc.h" namespace "zrpc":
    cdef cppclass _Application "zrpc::Application":
        _Application()
        _RpcChannel* CreateRpcChannel(string)


cdef class Application:
    cdef _Application *thisptr
    def __cinit__(self):
        self.thisptr = new _Application()
    def __dealloc__(self):
        del self.thisptr

    def CreateRpcChannel(self, endpoint):
        cdef RpcChannel channel = RpcChannel.__new__(RpcChannel)
        channel.thisptr = self.thisptr.CreateRpcChannel(make_string(endpoint))
        return channel
