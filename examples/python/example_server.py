#!/usr/bin/env python

import rpcz
import search_pb2
import search_rpcz

class SearchService(search_rpcz.SearchService):
  def Search(self, request, reply):
    print "Got request for '%s'" % request.query
    response = search_pb2.SearchResponse()
    response.results.append("result1 for " + request.query)
    response.results.append("this is result2")
    reply.send(response)

app = rpcz.Application()
server = app.create_server()
server.register_service(SearchService(), name="SearchService")
server.bind("tcp://*:5555")
print "Serving requests on port 5555"
app.run()
