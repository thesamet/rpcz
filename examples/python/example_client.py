#!/usr/bin/env python

import rpcz
import search_pb2
import search_rpcz
import sys
import time

app = rpcz.Application()

stub = search_rpcz.SearchService_Stub(
        app.CreateRpcChannel("tcp://127.0.0.1:5555"))

request = search_pb2.SearchRequest()
request.query = 'gold'
rpc = rpcz.RPC()
stub.Search(request, deadline_ms = 1000, rpc=rpc)
rpc.wait()
print "here"
