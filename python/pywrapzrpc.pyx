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
        string(char*)
        char* c_str()


cdef extern from "zrpc/rpc.h" namespace "zrpc":
    cdef cppclass _RPC "zrpc::RPC":
        bint OK()
        int GetStatus()
        char* GetErrorMessage()
        int GetApplicationError()
        int GetDeadlineMs()
        void SetDeadlineMs(int)


cdef class RPC:
    cdef _RPC *thisptr
    def __cinit__(self):
        self.thisptr = new _RPC()
    def __dealloc__(self):
        del self.thisptr
    def OK(self):
        return self.thisptr.OK()
    property status:
        def __get__(self):
            return self.thisptr.GetStatus()
    property application_error:
        def __get__(self):
            return self.thisptr.GetApplicationError()
    property deadline_ms:
        def __get__(self):
            return self.thisptr.GetDeadlineMs()
        def __set__(self, value):
            self.thisptr.SetDeadlineMs(value)


cdef extern from "zrpc/rpc_channel.h" namespace "zrpc":
    cdef cppclass _RpcChannel "zrpc::RpcChannel":
        pass


cdef class RpcChannel:
    cdef _RpcChannel *thisptr
    def __dealloc__(self):
        del self.thisptr
    def __init__(self):
        raise TypeError("Use a connection's MakeChannel to create a "
                        "RpcChannel.")


cdef extern from "zrpc/event_manager.h" namespace "zrpc":
    cdef cppclass _Connection "zrpc::Connection":
        _RpcChannel* MakeChannel()


cdef extern from "zrpc/event_manager.h" namespace "zrpc::Connection":
    _Connection* _CreateConnection "zrpc::Connection::CreateConnection" (_EventManager*, string)


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
