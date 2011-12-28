#!/usr/bin/env python

import zmq
import search_pb2
import search_rpcz
from rpcz import rpcz_pb2
from rpcz import connection


def main():
    context = zmq.Context(1)
    socket = context.socket(zmq.REQ);
    socket.connect('tcp://localhost:5555')
    conn = connection.BlockingConnection(socket)
    f = search_rpcz.SearchService_Stub(conn)
    request = search_pb2.SearchRequest()
    request.query = "bar"
    for y in xrange(1000):
        try:
            f.Search(request)
        except:
            pass


if __name__ == "__main__":
    main()
