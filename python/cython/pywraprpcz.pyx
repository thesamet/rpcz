from cpython cimport Py_DECREF, Py_INCREF
from libc.stdlib cimport malloc, free


cdef extern from "Python.h":
    void PyEval_InitThreads()


cdef extern from "rpcz/connection_manager.h" namespace "rpcz":
    cdef void InstallSignalHandler()


cdef extern from "rpcz/callback.h" namespace "rpcz":
  pass


def Init():
    import sys
    # InstallSignalHandler()
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


cdef string_ptr_to_pystring(string* s):
    return s.c_str()[:s.size()]


cdef cstring_to_pystring(void* s, size_t size):
    return (<char*>s)[:size]


cdef string_to_pystring(string s):
    return s.c_str()[:s.size()]


cdef extern from "rpcz/sync_event.h" namespace "rpcz":
    cdef cppclass _SyncEvent "rpcz::SyncEvent":
        void Signal() nogil
        void Wait() nogil

cdef extern from "rpcz/rpc.h" namespace "rpcz":
    cdef cppclass _RPC "rpcz::RPC":
        bint OK()
        int GetStatus()
        string GetErrorMessage()
        int GetApplicationErrorCode()
        long GetDeadlineMs()
        void SetDeadlineMs(long)
        int Wait() nogil


cdef class WrappedRPC:
    cdef _RPC *thisptr
    cdef _SyncEvent *sync_event

    def __cinit__(self):
        self.thisptr = new _RPC()
        self.sync_event = new _SyncEvent()
    def __dealloc__(self):
        del self.sync_event
        del self.thisptr
    def ok(self):
        return self.thisptr.OK()
    def wait(self):
        with nogil:
            self.sync_event.Wait()

    property status:
        def __get__(self):
            return self.thisptr.GetStatus()
    property application_error_code:
        def __get__(self):
            return self.thisptr.GetApplicationErrorCode()
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


# this function is called from C++ after we gave up the GIL. We use "with gil"
# to acquire it.
cdef void python_callback_bridge(ClosureWrapper *closure_wrapper) with gil:
    (<object>closure_wrapper.response_obj).ParseFromString(
            string_ptr_to_pystring(closure_wrapper.response_str))
    response = <object>closure_wrapper.response_obj;
    callback = <object>closure_wrapper.callback
    rpc = <WrappedRPC>closure_wrapper.rpc
    rpc.sync_event.Signal()
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
    def call_method(self, service_name, method_name,
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
                    python_callback_bridge, closure_wrapper))


cdef extern from "rpcz/service.h" namespace "rpcz":
  cdef cppclass _ServerChannel "rpcz::ServerChannel":
    void SendError(int, string)
    void Send0(string)
 

cdef class ServerChannel:
    cdef _ServerChannel *thisptr
    def __init__(self):
        raise TypeError("Do not initialize directly.")
    def __dealloc__(self):
        del self.thisptr
    def send_error(self, application_error_code, error_string=""):
      self.thisptr.SendError(application_error_code,
                             make_string(error_string))
      del self.thisptr
      self.thisptr = NULL
    def send(self, message):
      self.thisptr.Send0(make_string(message.SerializeToString()))
      del self.thisptr
      self.thisptr = NULL


ctypedef void(*Handler)(user_data, string method,
                        void* payload, size_t payload_len,
                        _ServerChannel* channel) nogil


cdef void rpc_handler_bridge(user_data, string& method,
                             void* payload, size_t payload_len,
                             _ServerChannel* channel) with gil:
  cdef ServerChannel channel_ = ServerChannel.__new__(ServerChannel)
  channel_.thisptr = channel
  user_data._call_method(string_to_pystring(method),
                         cstring_to_pystring(payload, payload_len),
                         channel_)


cdef extern from "python_rpc_service.h" namespace "rpcz":
    cdef cppclass PythonRpcService:
        PythonRpcService(Handler, object)



cdef extern from "rpcz/rpcz.h" namespace "rpcz":
    cdef cppclass _Server "rpcz::Server":
        void RegisterService(PythonRpcService*, string name)
        void Bind(string endpoint)


cdef class Server:
    cdef _Server *thisptr
    def __init__(self):
        raise TypeError("Use Application.CreateServer to create a "
                        "Server.")
    def __dealloc__(self):
        del self.thisptr
    def register_service(self, service, name=None):
        cdef PythonRpcService* rpc_service = new PythonRpcService(
            rpc_handler_bridge, service)
        self.thisptr.RegisterService(rpc_service, make_string(name))
    def bind(self, endpoint):
        self.thisptr.Bind(make_string(endpoint))

cdef extern from "rpcz/rpcz.h" namespace "rpcz":
    cdef cppclass _Application "rpcz::Application":
        _Application()
        _RpcChannel* CreateRpcChannel(string)
        _Server* CreateServer()
        void Terminate()
        void Run() nogil


cdef class Application:
    cdef _Application *thisptr
    def __cinit__(self):
        self.thisptr = new _Application()
    def __dealloc__(self):
        del self.thisptr

    def create_rpc_channel(self, endpoint):
        cdef RpcChannel channel = RpcChannel.__new__(RpcChannel)
        channel.thisptr = self.thisptr.CreateRpcChannel(make_string(endpoint))
        return channel

    def create_server(self):
        cdef Server server = Server.__new__(Server)
        server.thisptr = self.thisptr.CreateServer()
        return server

    def terminate(self):
        self.thisptr.Terminate()

    def run(self):
        with nogil:
            self.thisptr.Run()

