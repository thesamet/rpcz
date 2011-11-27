#!/usr/bin/env python

import zmq
import search_pb2
import search_zrpc
from zrpc import zrpc_pb2
from zrpc import connection


def main():
    context = zmq.Context(1)
    socket = context.socket(zmq.REQ);
    socket.connect('tcp://localhost:5555')
    conn = connection.BlockingConnection(socket)
    f = search_zrpc.SearchService_Stub(conn)
    request = search_pb2.SearchRequest()
    request.query = "bar"
    print f.Search(request)


if __name__ == "__main__":
    main()
