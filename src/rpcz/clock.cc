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

#include "rpcz/clock.hpp"
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace rpcz {

uint64 zclock_time(void) {
#ifdef WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
	// ft now contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
	ULARGE_INTEGER temp;
	temp.HighPart = ft.dwHighDateTime;
	temp.LowPart = ft.dwLowDateTime;
    return (uint64) (temp.QuadPart / 10000);
#else
	struct timeval tv;
    gettimeofday (&tv, NULL);
    return (uint64) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

}  // namespace rpcz
