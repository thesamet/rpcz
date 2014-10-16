RPCZ: Protocol Buffer RPC transport
===================================

RPC implementation for Protocol Buffers over ZeroMQ

Introduction
------------


  * RPCZ is a library for writing fast and robust RPC clients and servers that speak Protocol Buffers.

  * RPCZ currently supports writing clients and servers in C++ and Python. More languages may be added in the future.

  * The API offers both asynchronous (callbacks) and synchronous (blocking) style functionality. Both styles allow specifying deadlines in millisecond resolution.

  * RPCZ is built on top of [ZeroMQ](http://www.zeromq.org/) for handling the low-level I/O details in a lock-free manner.

  * The Python module is a [Cython](http://www.cython.org/) wrapper around the C++ API.

  * RPCZ has been tested on Ubuntu 11.10 and Mac OS X Lion. I believe it should not be hard to get it to compile on Windows but I have not tried.

Quick Examples (API show off)
-----------------------------

Let's write a C++ server and a Python client for a search service defined as follows:

```protobuf
message SearchRequest {
  required string query = 1;
  optional int32 page_number = 2 [default = 1];
}

message SearchResponse {
  repeated string results = 1;
}

service SearchService {
  rpc Search(SearchRequest) returns(SearchResponse);
}
```

Example: Python Client
----------------------
[Source code](https://github.com/thesamet/rpcz/tree/master/examples/cpp)

```python
app = rpcz.Application()

stub = search_rpcz.SearchService_Stub(
        app.create_rpc_channel("tcp://127.0.0.1:5555"))

request = search_pb2.SearchRequest()
request.query = 'gold'
print stub.Search(request, deadline_ms=1000)
```


Example: C++ Server
-------------------

[Source code](https://github.com/thesamet/rpcz/tree/master/examples/cpp)

```cpp
class SearchServiceImpl : public SearchService {
  virtual void Search(
      const SearchRequest& request,
      rpcz::Reply<SearchResponse> reply) {
    SearchResponse response;
    response.add_results("result1 for " + request.query());
    response.add_results("this is result2");
    reply.Send(response);
  }
};

int main() {
  rpcz::Application application;
  rpcz::Server* server = application.CreateServer("tcp://*:5555");
  examples::SearchServiceImpl search_service;
  server->RegisterService(&search_service);
  server->Start();
  delete server;
}
```

Getting Started: Installing
---------------------------

  * Make sure you have RPCZ's dependencies installed: Protocol Buffers (duh!), ZeroMQ, Boost (threads and program_options only), and CMake. If you are on Ubuntu:
```bash
apt-get install libprotobuf-dev libprotoc-dev libzmq-dev libboost-thread-dev libboost-program-options-dev cmake
```

  * Download, build and install:
```bash
git clone https://code.google.com/p/rpcz/
cd rpcz
mkdir build
cd build
cmake .. -Drpcz_build_examples=1
make
sudo make install
```

You don't really have to `make install` if you don't want to. Just make sure that when you compile your code, your compiler is aware of RPCZ's include and library directories.

  * Python support (optional):
```bash
cd ../python
python setup.py build
pip install -e .
```
  If you are on Linux and Python complains that it can not find `librpcz.so`, then you may have to run `sudo ldconfig`. If you have not installed the library to a standard location,  you can build the Python module with `--rpath` to hard-code `librpcz.so`'s path  in the Python module:
```bash
python setup.py build_ext -f --rpath=/path/to/build/src/rpcz
pip install -e .
```

Generating Client and Server classes
------------------------------------

RPCZ comes with a protoc plugins that generate client and server code in C++ and Python. They are used only to generate the service code. The message serialization and parsing is still done by the original Protocol Buffer implementation.

To generate C++ RPCZ classes:
```bash
protoc -I=$SRC_DIR --cpp_rpcz_out=$DST_DIR $SRC_DIR/search.proto
```
Similarly, to generate Python RPCZ classes:
```bash
protoc -I=$SRC_DIR --python_rpcz_out=$DST_DIR $SRC_DIR/search.proto
```

If protoc can not find the plugin, you can help it by appending `--protoc-gen-cpp_rpcz=/path/to/bin/protoc-gen-cpp_rpcz` to the above command (change `cpp` above to `python` if you are generating Python code)

