// Copyright 2011 Google Inc. All Rights Reserved.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: nadavs@google.com <Nadav Samet>

#include "zrpc/event_manager.h"
#include <zmq.hpp>
#include "zrpc/macros.h"
#include "gtest/gtest.h"


namespace zrpc {

class EventManagerTest : public ::testing::Test {
 public:
  EventManagerTest() : context_(1) {}
 protected:
  zmq::context_t context_;
};

TEST_F(EventManagerTest, DoesFoo) {
  EventManager em(&context_, 10);
}
}  // namespace zrpc

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
