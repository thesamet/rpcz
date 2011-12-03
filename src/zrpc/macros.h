// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#ifndef ZRPC_BASE_H
#define ZRPC_BASE_H

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
    void operator=(const TypeName&)

template<typename IteratorType>
void DeleteContainerPointers(const IteratorType& begin,
                             const IteratorType& end) {
  for (IteratorType i = begin; i != end; ++i) {
    delete *i;
  }
}

#endif
