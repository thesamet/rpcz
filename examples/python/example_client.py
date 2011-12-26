#!/usr/bin/env python

import zrpc
import search_pb2
import search_zrpc
import sys
import time


app = zrpc.Application()
stub = search_zrpc.SearchService_Stub(
        app.CreateRpcChannel("tcp://127.0.0.1:5555"))
request = search_pb2.SearchRequest()
response = search_pb2.SearchResponse()
request.query = 'gold'
rpc = zrpc.RPC()
rpc.deadline_ms = 1000
stub.Search(rpc, request, response, None)
rpc.wait()
print response
