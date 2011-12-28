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

cdef extern from "rpcz/connection_manager.h" namespace "rpcz":
    cdef void InstallSignalHandler()

cdef extern from "rpcz/callback.h" namespace "rpcz":
  pass

def Init():
    import sys
    # InstallFailureSignalHandler()
    # InitGoogleLogging(sys.argv[0])
    InstallSignalHandler()
    PyEval_InitThreads()


Init()


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


# cdef extern from "rpcz/rpc.h" namespace "rpcz":
#     cdef enum Status "rpcz::GeneratedRPCResponse::Status":
#         APPLICATION_ERROR, OK, DEADLINE_EXCEEDED


cdef extern from "rpcz/rpc.h" namespace "rpcz":
    cdef cppclass _RPC "rpcz::RPC":
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
    void* rpc


cdef extern from "rpcz/macros.h" namespace "rpcz":
    cdef cppclass Closure:
        pass

    Closure* NewCallback(void(ClosureWrapper*) nogil, ClosureWrapper*)

cdef void PythonCallbackBridge(ClosureWrapper *closure_wrapper) with gil:
    # We don't really have the GIL when this function is called, and it is
    # ran from a non-python thread.
    (<object>closure_wrapper.response_obj).ParseFromString(
            ptr_string_to_pystring(closure_wrapper.response_str))
    response = <object>closure_wrapper.response_obj;
    callback = <object>closure_wrapper.callback
    rpc = <object>closure_wrapper.rpc
    if callback is not None:
        callback(response, rpc)
    Py_DECREF(<object>closure_wrapper.response_obj)
    Py_DECREF(<object>closure_wrapper.callback)
    Py_DECREF(<object>closure_wrapper.rpc)
    del closure_wrapper.response_str
    free(closure_wrapper)


cdef extern from "rpcz/rpc_channel.h" namespace "rpcz":
    cdef cppclass _RpcChannel "rpcz::RpcChannel":
        void CallMethod0(string service_name, string method_name,
                         string request, string* response, _RPC* rpc, 
                         Closure* callback) except +


cdef class RpcChannel:
    cdef _RpcChannel *thisptr
    def __dealloc__(self):
        del self.thisptr
    def __init__(self):
        raise TypeError("Use Application.CreateRpcChannel to create a "
                        "RpcChannel.")
    def CallMethod(self, service_name, method_name,
                   request, response, WrappedRPC rpc, callback):
        cdef ClosureWrapper* closure_wrapper = <ClosureWrapper*>malloc(
                sizeof(ClosureWrapper))
        closure_wrapper.response_str = new string()
        closure_wrapper.response_obj = <void*>response
        closure_wrapper.callback = <void*>callback
        closure_wrapper.rpc = <void*>rpc
        Py_INCREF(response)
        Py_INCREF(callback)
        Py_INCREF(rpc)
        self.thisptr.CallMethod0(
                make_string(service_name),
                make_string(method_name),
                make_string(request.SerializeToString()),
                closure_wrapper.response_str,
                rpc.thisptr,
                NewCallback(
                    PythonCallbackBridge, closure_wrapper))


cdef extern from "rpcz/rpcz.h" namespace "rpcz":
    cdef cppclass _Application "rpcz::Application":
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
