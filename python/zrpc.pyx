cdef extern from "zrpc/rpc.h" namespace "zrpc":
    cdef cppclass RPC:
        bint OK()
        int GetStatus()
        char* GetErrorMessage()
        int GetApplicationError()
        int GetDeadlineMs()
        void SetDeadlineMs(int)
