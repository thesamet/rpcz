from cpython cimport Py_DECREF, Py_INCREF
from libc.stdlib cimport malloc, free


cdef extern from "glog/logging.h" namespace "google":
    cdef void InstallFailureSignalHandler()
    cdef void InitGoogleLogging(char*)


def Init():
    import sys
    InstallFailureSignalHandler()
    InitGoogleLogging(sys.argv[0])


cdef extern from "zrpc/event_manager.h" namespace "zrpc":
    cdef cppclass _EventManager "zrpc::EventManager":
        _EventManager(int)
        int GetThreadCount()


cdef class EventManager:
    cdef _EventManager *thisptr
    def __cinit__(self):
        self.thisptr = new _EventManager(1)
    def __dealloc__(self):
        del self.thisptr
    property thread_count:
        def __get__(self):
            return self.thisptr.GetThreadCount()


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
        return value

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
    (<object>closure_wrapper.response_obj).ParseFromString(
            ptr_string_to_pystring(closure_wrapper.response_str))
    (<object>closure_wrapper.callback)()
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
        raise TypeError("Use a connection's MakeChannel to create a "
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


cdef extern from "zrpc/event_manager.h" namespace "zrpc":
    cdef cppclass _Connection "zrpc::Connection":
        _RpcChannel* MakeChannel()

    _Connection* _CreateConnection "zrpc::Connection::CreateConnection" (
            _EventManager*, string)


cdef class Connection:
    cdef _Connection *thisptr
    def __dealloc__(self):
        del self.thisptr
    def __init__(self):
        raise TypeError("Use CreateConnection() to create a connection "
                        "object.")
    def MakeChannel(self):
        cdef RpcChannel channel = RpcChannel.__new__(RpcChannel)
        channel.thisptr = self.thisptr.MakeChannel()
        return channel


def CreateConnection(EventManager em, endpoint):
    cdef Connection c = Connection.__new__(Connection)
    c.thisptr = _CreateConnection(em.thisptr, string(endpoint))
    return c
