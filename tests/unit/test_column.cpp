#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "sillyql/column.hpp"

namespace sillyql {
namespace {

std::string cellText(const Column& column, RowId row) {
  std::ostringstream os;
  column.write(os, row);
  return os.str();
}

TEST(ColumnTest, StartsEmptyWithItsType) {
  for (const ColumnType type :
       {ColumnType::Int, ColumnType::Double, ColumnType::Bool, ColumnType::String}) {
    const Column column(type);
    EXPECT_EQ(column.type(), type);
    EXPECT_EQ(column.size(), 0U);
  }
}

TEST(ColumnTest, AppendsValuesAndReadsThemBack) {
  Column column(ColumnType::Double);
  column.append(Value(1.5));
  column.append(Value(-2.0));
  ASSERT_EQ(column.size(), 2U);
  EXPECT_EQ(column.valueAt(0), Value(1.5));
  EXPECT_EQ(column.valueAt(1), Value(-2.0));
  EXPECT_EQ(column.values<double>(), (std::vector<double>{1.5, -2.0}));
}

TEST(ColumnTest, AppendRejectsValueOfAnotherType) {
  Column column(ColumnType::Int);
  EXPECT_THROW(column.append(Value("7")), TypeMismatchError);
  EXPECT_EQ(column.size(), 0U);
}

TEST(ColumnTest, StoresBoolsAsBytes) {
  Column column(ColumnType::Bool);
  column.append(Value(true));
  column.append(Value(false));
  EXPECT_EQ(column.values<BoolStorage>(), (std::vector<BoolStorage>{1, 0}));
  EXPECT_EQ(column.valueAt(0), Value(true));
  EXPECT_EQ(cellText(column, 0), "true");
  EXPECT_EQ(cellText(column, 1), "false");
}

TEST(ColumnTest, AppendTextParsesByTypeAndRejectsBadLiterals) {
  Column ints(ColumnType::Int);
  EXPECT_TRUE(ints.appendText("12"));
  EXPECT_FALSE(ints.appendText("twelve"));
  EXPECT_FALSE(ints.appendText("1.5"));
  EXPECT_EQ(ints.values<std::int32_t>(), (std::vector<std::int32_t>{12}));

  Column strings(ColumnType::String);
  EXPECT_TRUE(strings.appendText("x,y"));
  EXPECT_EQ(strings.valueAt(0), Value("x,y"));
}

TEST(ColumnTest, AppendTextRejectsStringsThatAreNotOneToken) {
  Column strings(ColumnType::String);
  std::string spaced = "two words";
  EXPECT_FALSE(strings.appendText(std::move(spaced)));
  EXPECT_EQ(spaced, "two words");  // NOLINT(bugprone-use-after-move): kept on failure.
  EXPECT_FALSE(strings.appendText(""));
  EXPECT_EQ(strings.size(), 0U);
}

TEST(ColumnTest, WriteUsesEngineFormatting) {
  Column column(ColumnType::Double);
  column.append(Value(63901.00));
  column.append(Value(46700.95));
  EXPECT_EQ(cellText(column, 0), "63901");
  EXPECT_EQ(cellText(column, 1), "46700.9");
}

TEST(ColumnTest, AppendFromMovesAllElements) {
  Column target(ColumnType::String);
  target.append(Value("a"));
  Column source(ColumnType::String);
  source.append(Value("b"));
  source.append(Value("c"));
  target.appendFrom(std::move(source));
  EXPECT_EQ(target.values<std::string>(), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(ColumnTest, AppendFromIntoANewColumnAdoptsTheBuffer) {
  Column source(ColumnType::Double);
  source.append(Value(1.0));
  source.append(Value(2.0));
  const double* buffer = source.values<double>().data();
  Column target(ColumnType::Double);
  target.appendFrom(std::move(source));
  EXPECT_EQ(target.values<double>().data(), buffer);  // O(1): no element was copied.
  EXPECT_EQ(target.values<double>(), (std::vector<double>{1.0, 2.0}));
  EXPECT_EQ(source.size(), 0U);  // NOLINT(bugprone-use-after-move): documented to be empty.
}

TEST(ColumnTest, ReserveForMakesAppendFromNonAllocating) {
  Column target(ColumnType::Int);
  target.append(Value(1));
  Column source(ColumnType::Int);
  for (const std::int32_t value : {2, 3, 4}) {
    source.append(Value(value));
  }
  target.reserveFor(source);
  ASSERT_GE(target.values<std::int32_t>().capacity(), 4U);
  const std::int32_t* buffer = target.values<std::int32_t>().data();
  target.appendFrom(std::move(source));
  EXPECT_EQ(target.values<std::int32_t>().data(), buffer);
  EXPECT_EQ(target.values<std::int32_t>(), (std::vector<std::int32_t>{1, 2, 3, 4}));
}

TEST(ColumnTest, TruncateKeepsAPrefix) {
  Column column(ColumnType::String);
  for (const char* text : {"a", "b", "c"}) {
    column.append(Value(text));
  }
  column.truncate(5);
  EXPECT_EQ(column.size(), 3U);
  column.truncate(1);
  EXPECT_EQ(column.values<std::string>(), (std::vector<std::string>{"a"}));
}

TEST(ColumnTest, AppendFromRejectsDifferentType) {
  Column target(ColumnType::Int);
  EXPECT_THROW(target.appendFrom(Column(ColumnType::Double)), TypeMismatchError);
}

TEST(ColumnTest, CompactKeepsMarkedRowsInOrder) {
  Column column(ColumnType::String);
  for (const char* text : {"r0", "r1", "r2", "r3", "r4"}) {
    column.append(Value(text));
  }
  const std::vector<std::uint8_t> keep{0, 1, 0, 1, 1};
  column.compact(keep);
  EXPECT_EQ(column.values<std::string>(), (std::vector<std::string>{"r1", "r3", "r4"}));
}

TEST(ColumnTest, VisitDispatchesOnStorageType) {
  Column column(ColumnType::Int);
  column.append(Value(3));
  column.append(Value(4));
  const ColumnType visited =
      column.visit([](const auto& values) { return columnTypeOf<ElementOf<decltype(values)>>(); });
  EXPECT_EQ(visited, ColumnType::Int);
  const std::size_t count = column.visit([](const auto& values) { return values.size(); });
  EXPECT_EQ(count, 2U);
}

TEST(StorageTest, ParseValueTypesLiteralsByColumnType) {
  EXPECT_EQ(parseValue(ColumnType::Int, "42"), Value(42));
  EXPECT_EQ(parseValue(ColumnType::Double, "42"), Value(42.0));
  EXPECT_EQ(parseValue(ColumnType::Bool, "false"), Value(false));
  EXPECT_EQ(parseValue(ColumnType::String, "42"), Value("42"));
  EXPECT_FALSE(parseValue(ColumnType::Int, "4.2"));
  EXPECT_FALSE(parseValue(ColumnType::Bool, "1"));
  EXPECT_FALSE(parseValue(ColumnType::String, ""));
}

TEST(StorageTest, VisitStorageTypeSelectsTheElementType) {
  for (const ColumnType type :
       {ColumnType::Int, ColumnType::Double, ColumnType::Bool, ColumnType::String}) {
    EXPECT_EQ(visitStorageType(
                  type, [](auto tag) { return columnTypeOf<typename decltype(tag)::type>(); }),
              type);
  }
}

TEST(StorageTest, ConvertsBetweenValuesAndStorage) {
  EXPECT_EQ(storageKey<BoolStorage>(Value(true)), 1);
  EXPECT_EQ(storageKey<std::string>(Value("k")), "k");
  EXPECT_THROW((void)storageKey<double>(Value(1)), TypeMismatchError);
  EXPECT_EQ(toValue(BoolStorage{0}), Value(false));
  EXPECT_EQ(toValue(std::int32_t{9}), Value(9));
  static_assert(columnTypeOf<BoolStorage>() == ColumnType::Bool);
  static_assert(columnTypeOf<std::string>() == ColumnType::String);
}

}  // namespace
}  // namespace sillyql
