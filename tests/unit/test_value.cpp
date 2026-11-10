#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "sillyql/value.hpp"

namespace sillyql {
namespace {

std::string print(const Value& value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

TEST(ValueTest, ReportsTypeOfEachAlternative) {
  EXPECT_EQ(Value(7).type(), ColumnType::Int);
  EXPECT_EQ(Value(2.5).type(), ColumnType::Double);
  EXPECT_EQ(Value(true).type(), ColumnType::Bool);
  EXPECT_EQ(Value("text").type(), ColumnType::String);
  EXPECT_EQ(Value(std::string("text")).type(), ColumnType::String);
}

TEST(ValueTest, GetReturnsStoredValueAndRejectsOtherTypes) {
  EXPECT_EQ(Value(7).get<std::int32_t>(), 7);
  EXPECT_EQ(Value("abc").get<std::string>(), "abc");
  EXPECT_THROW((void)Value(7).get<double>(), TypeMismatchError);
  EXPECT_THROW((void)Value(true).get<std::int32_t>(), TypeMismatchError);
}

TEST(ValueTest, EqualityIsFalseAcrossTypes) {
  EXPECT_EQ(Value(1), Value(1));
  EXPECT_NE(Value(1), Value(2));
  EXPECT_NE(Value(1), Value(1.0));
  EXPECT_NE(Value(1), Value(true));
  EXPECT_EQ(Value("a"), Value(std::string("a")));
}

TEST(ValueTest, OrdersValuesOfTheSameType) {
  EXPECT_LT(Value(-3), Value(2));
  EXPECT_LT(Value(1.5), Value(1.75));
  EXPECT_LT(Value(false), Value(true));
  EXPECT_LT(Value("Zed"), Value("apple"));  // Byte-wise: uppercase sorts first.
  EXPECT_GT(Value("b"), Value("abc"));
  EXPECT_LE(Value(4), Value(4));
}

TEST(ValueTest, OrderingAcrossTypesThrows) {
  EXPECT_THROW((void)(Value(1) < Value(1.0)), TypeMismatchError);
  EXPECT_THROW((void)(Value("1") > Value(1)), TypeMismatchError);
}

TEST(ValueTest, PrintsWithDefaultStreamFormatting) {
  EXPECT_EQ(print(Value(-17)), "-17");
  EXPECT_EQ(print(Value(25965.00)), "25965");
  EXPECT_EQ(print(Value(46.87)), "46.87");
  EXPECT_EQ(print(Value(1234567.5)), "1.23457e+06");
  EXPECT_EQ(print(Value(0.000012345)), "1.2345e-05");
  EXPECT_EQ(print(Value(-0.0)), "-0");
  EXPECT_EQ(print(Value(true)), "true");
  EXPECT_EQ(print(Value(false)), "false");
  EXPECT_EQ(print(Value("Ann_Arbor")), "Ann_Arbor");
}

TEST(ColumnTypeTest, NamesRoundTrip) {
  for (const ColumnType type :
       {ColumnType::Int, ColumnType::Double, ColumnType::Bool, ColumnType::String}) {
    EXPECT_EQ(parseColumnType(columnTypeName(type)), type);
  }
  EXPECT_FALSE(parseColumnType("float"));
  EXPECT_FALSE(parseColumnType("Int"));
}

}  // namespace
}  // namespace sillyql
