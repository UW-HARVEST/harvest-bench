// Tests of the toy library's C API, linked from TEST_LIB_PATH.
#include <gtest/gtest.h>

#include <string>

extern "C" {
#include "toy.h"
}

TEST(ToyLib, UpperCountsChanges) {
  std::string s = "abC1z";
  EXPECT_EQ(toy_upper(s.data(), s.size()), 3u);
  EXPECT_EQ(s, "ABC1Z");
}
