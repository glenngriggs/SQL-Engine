#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include "sillyql/literal.hpp"

namespace sillyql {
namespace {

TEST(LiteralTest, ParsesIntegersLikeStreamExtraction) {
  EXPECT_EQ(parseInt("0"), 0);
  EXPECT_EQ(parseInt("-2147483648"), std::numeric_limits<std::int32_t>::min());
  EXPECT_EQ(parseInt("2147483647"), std::numeric_limits<std::int32_t>::max());
  EXPECT_EQ(parseInt("+15"), 15);
  EXPECT_EQ(parseInt("007"), 7);
  EXPECT_FALSE(parseInt("2147483648"));
  EXPECT_FALSE(parseInt(""));
  EXPECT_FALSE(parseInt("+"));
  EXPECT_FALSE(parseInt("+-5"));
  EXPECT_FALSE(parseInt("12abc"));
  EXPECT_FALSE(parseInt("1.0"));
}

TEST(LiteralTest, ParsesDoublesLikeStreamExtraction) {
  EXPECT_EQ(parseDouble("2.5"), 2.5);
  EXPECT_EQ(parseDouble("-.5"), -0.5);
  EXPECT_EQ(parseDouble("5."), 5.0);
  EXPECT_EQ(parseDouble("+1e3"), 1000.0);
  EXPECT_EQ(parseDouble("7"), 7.0);
  EXPECT_FALSE(parseDouble("inf"));
  EXPECT_FALSE(parseDouble("nan"));
  EXPECT_FALSE(parseDouble("-infinity"));
  EXPECT_FALSE(parseDouble("0x10"));
  EXPECT_FALSE(parseDouble("1e"));
  EXPECT_FALSE(parseDouble(""));
}

TEST(LiteralTest, DoubleUnderflowReadsAsSignedZeroButOverflowIsRejected) {
  const double tiny = parseDouble("1e-400").value_or(1.0);
  EXPECT_EQ(tiny, 0.0);
  EXPECT_FALSE(std::signbit(tiny));

  const double negativeTiny = parseDouble("-1e-400").value_or(1.0);
  EXPECT_EQ(negativeTiny, 0.0);
  EXPECT_TRUE(std::signbit(negativeTiny));

  EXPECT_EQ(parseDouble("1e-310"), 1e-310);  // Subnormal, still representable.
  EXPECT_FALSE(parseDouble("1e999"));
  EXPECT_FALSE(parseDouble("-1.8e308"));
  EXPECT_FALSE(parseDouble("1e-400x"));
}

TEST(LiteralTest, ParsesBoolsAndCounts) {
  EXPECT_EQ(parseBool("true"), true);
  EXPECT_EQ(parseBool("false"), false);
  EXPECT_FALSE(parseBool("TRUE"));
  EXPECT_FALSE(parseBool("truex"));
  EXPECT_EQ(parseCount("12"), 12U);
  EXPECT_FALSE(parseCount("-1"));
  EXPECT_FALSE(parseCount("three"));
}

TEST(LiteralTest, StringLiteralIsOneNonEmptyToken) {
  EXPECT_TRUE(isStringLiteral("Ann_Arbor"));
  EXPECT_TRUE(isStringLiteral("x,y"));
  EXPECT_TRUE(isStringLiteral("#tag"));
  EXPECT_FALSE(isStringLiteral(""));
  EXPECT_FALSE(isStringLiteral("x, y"));
  EXPECT_FALSE(isStringLiteral("tab\there"));
  EXPECT_FALSE(isStringLiteral("line\n"));
}

}  // namespace
}  // namespace sillyql
