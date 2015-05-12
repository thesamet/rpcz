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

  * RPCZ has been tested on Ubuntu 11.10, Ubuntu 14.04, Mac OS X Lion and Microsoft Visual Studio 2012.

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
      rpcz::reply<SearchResponse> reply) {
    cout << "Got request for '" << request.query() << "'" << endl;
    SearchResponse response;
    response.add_results("result1 for " + request.query());
    response.add_results("this is result2");
    reply.send(response);
  }
};

int main() {
  rpcz::application application;
  rpcz::server server(application);
  SearchServiceImpl search_service;
  server.register_service(&search_service);
  cout << "Serving requests on port 5555." << endl;
  server.bind("tcp://*:5555");
  application.run();
}

```

Getting Started: Installing on Linux
------------------------------------

  * Make sure you have RPCZ's dependencies installed: Protocol Buffers (duh!), ZeroMQ, Boost (threads and program_options), and CMake. If you are on Ubuntu:
```bash
apt-get install libprotobuf-dev libprotoc-dev libzmq-dev \
        libboost-thread-dev libboost-program-options-dev cmake
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

  * Build Debian package:

Instead of using `make install`, you can install RPCZ with the debian package. Once your build is completed, you can generate the debian package using:

```bash
make package
```

The package runtime dependencies have be tuned for Ubuntu 14.04 so YMMV. You can adapt these dependencies for your distribution by changing the `CPACK_DEBIAN_PACKAGE_DEPENDS` variable in the top-level `CMakeLists.txt` file.

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

Getting Started: Installing on Windows
--------------------------------------

First of all, on Windows it is recommended to build both release and debug configurations in order to avoid mixing runtime libraries. You will have to use the CMake GUI tool to generate the Microsoft Visual Studio project. There's many way to setup dependencies but here is a recipe that worked for me.

* Boost installation
   1. Download and install Boost binaries from the [sourceforge web page](http://sourceforge.net/projects/boost/). On windows, you will need `date_time` in addition to `threads` and `program_options`.
   2. Set the `BOOST_ROOT` environment variable to the installation path used above.
   3. Add the boost DLL directory to your `PATH` (e.g. `%BOOST_ROOT%\lib32-msvc-11.0`).
* Google Protobuf installation
   1. Download and extract the protobuf source package [protobuf-2.6.1.zip](https://github.com/google/protobuf/releases/download/v2.6.1/protobuf-2.6.1.zip) in the final installation directory.
   2. Open the Visual Studio solution in the `vsprojects` subdirectory and build both debug and release configurations.
   3. Set the `PROTOBUF_SRC_ROOT_FOLDER` environment variable to the protobuf directory used above.
* ZeroMQ installation
   1. Download and install the [ZeroMQ binaries](http://zeromq.org/distro:microsoft-windows).
   2. Download the [ZeroMQ C++ bindings](https://github.com/zeromq/cppzmq) and copy the `zmq.hpp` header file in the `include` directory of ZeroMQ.
   3. Set the `ZEROMQ_ROOT`environment variable to the installation path used above.
   4. Add the ZeroMQ DLL directory to your `PATH` (e.g. `%ZEROMQ_ROOT%\bin`).

Then, you are ready to configure and generate the Microsoft Visual Studio project using CMake.

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

