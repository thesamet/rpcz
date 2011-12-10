cdef extern from "zrpc/rpc.h" namespace "zrpc":
    cdef cppclass RPC:
        bool OK()
        int GetStatus()
        std::string GetErrorMessage()
        int GetApplicationError()
        int GetDeadlineMs()
        void SetDeadlineMs(int)
