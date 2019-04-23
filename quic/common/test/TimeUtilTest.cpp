/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/common/TimeUtil.h>
#include <gtest/gtest.h>

namespace quic {
namespace test {

using namespace std;
using namespace quic;
using namespace testing;

TEST(TimeUtil, TestMinTwo) {
  std::chrono::milliseconds ms1 = std::chrono::milliseconds(10);
  std::chrono::milliseconds ms2 = std::chrono::milliseconds(20);
  EXPECT_EQ(timeMin(ms1, ms2).count(), 10);
}

TEST(TimeUtil, TestMinFive) {
  std::chrono::milliseconds ms1 = std::chrono::milliseconds(20);
  std::chrono::milliseconds ms2 = std::chrono::milliseconds(30);
  std::chrono::milliseconds ms3 = std::chrono::milliseconds(40);
  std::chrono::milliseconds ms4 = std::chrono::milliseconds(10);
  EXPECT_EQ(timeMin(ms1, ms2, ms3, ms4).count(), 10);
}

TEST(TimeUtil, TestMaxTwo) {
  std::chrono::milliseconds ms1 = std::chrono::milliseconds(10);
  std::chrono::milliseconds ms2 = std::chrono::milliseconds(20);
  EXPECT_EQ(timeMax(ms1, ms2).count(), 20);
}

TEST(TimeUtil, TestMaxFive) {
  std::chrono::milliseconds ms1 = std::chrono::milliseconds(20);
  std::chrono::milliseconds ms2 = std::chrono::milliseconds(30);
  std::chrono::milliseconds ms3 = std::chrono::milliseconds(40);
  std::chrono::milliseconds ms4 = std::chrono::milliseconds(10);
  EXPECT_EQ(timeMax(ms1, ms2, ms3, ms4).count(), 40);
}
} // namespace test
} // namespace quic