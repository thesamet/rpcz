// Copyright 2011, Nadav Samet.
// All rights reserved.
//
// Author: thesamet@gmail.com <Nadav Samet>

#include "zrpc/callback.h"

#include <string>
#include "gtest/gtest.h"

namespace zrpc {

static bool called = false;
void ZeroCallback() {
  called = true;
}

class TestObject {
 public:
  void Method(int arg1, std::string arg2) {
    EXPECT_EQ(17, arg1);
    EXPECT_EQ("super!", arg2);
    called = true;
  }
};

TEST(CallbackTest, TestCallbackSimple) {
  Closure* c = NewCallback(&ZeroCallback);
  called = false;
  c->Run();
  EXPECT_TRUE(called);
}

TEST(CallbackTest, TestPermanentCallbackSimple) {
  Closure* c = NewPermanentCallback(&ZeroCallback);
  for (int i = 1; i < 10; ++i) {
    called = false;
    c->Run();
    EXPECT_TRUE(called);
  }
  delete c;
  EXPECT_TRUE(called);
}

TEST(CallbackTest, TestMethodCallback) {
  TestObject object;
  called = false;
  Closure *c = NewCallback(&object, &TestObject::Method, 17,
                           std::string("super!"));
  c->Run();
  EXPECT_TRUE(called);
}

TEST(CallbackTest, TestMethodPermanentCallback) {
  TestObject object;
  called = false;
  Closure *c = NewPermanentCallback(&object, &TestObject::Method, 17,
                                    std::string("super!"));
  for (int i = 1; i < 10; ++i) {
    called = false;
    c->Run();
    EXPECT_TRUE(called);
  }
  delete c;
  EXPECT_TRUE(called);
}
}  // namespace zrpc
