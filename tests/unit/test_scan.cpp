#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "sillyql/scan.hpp"

namespace sillyql {
namespace {

TEST(ScanKernelTest, EachOperatorSelectsMatchingRowsInOrder) {
  const std::vector<std::int32_t> values{5, 1, 5, 9, 3};
  const std::span<const std::int32_t> view(values);
  EXPECT_EQ(scanValues(view, CompareOp::Less, 5), (std::vector<RowId>{1, 4}));
  EXPECT_EQ(scanValues(view, CompareOp::LessEqual, 5), (std::vector<RowId>{0, 1, 2, 4}));
  EXPECT_EQ(scanValues(view, CompareOp::Equal, 5), (std::vector<RowId>{0, 2}));
  EXPECT_EQ(scanValues(view, CompareOp::NotEqual, 5), (std::vector<RowId>{1, 3, 4}));
  EXPECT_EQ(scanValues(view, CompareOp::GreaterEqual, 5), (std::vector<RowId>{0, 2, 3}));
  EXPECT_EQ(scanValues(view, CompareOp::Greater, 5), (std::vector<RowId>{3}));
}

TEST(ScanKernelTest, SelectionSpansBlockBoundaries) {
  std::vector<std::int32_t> values(2'500);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<std::int32_t>(i % 3);
  }
  const std::vector<RowId> rows =
      scanValues(std::span<const std::int32_t>(values), CompareOp::Equal, 0);
  ASSERT_EQ(rows.size(), 834U);
  for (std::size_t i = 0; i < rows.size(); ++i) {
    EXPECT_EQ(rows[i], 3 * i);
  }
}

TEST(ScanKernelTest, ScanColumnDispatchesOnType) {
  Column flags(ColumnType::Bool);
  for (const bool flag : {true, false, true}) {
    flags.append(Value(flag));
  }
  EXPECT_EQ(scanColumn(flags, CompareOp::Equal, Value(true)), (std::vector<RowId>{0, 2}));
  EXPECT_EQ(scanColumn(flags, CompareOp::Less, Value(true)), (std::vector<RowId>{1}));
  EXPECT_THROW((void)scanColumn(flags, CompareOp::Equal, Value(1)), TypeMismatchError);
  EXPECT_EQ(allRows(3), (std::vector<RowId>{0, 1, 2}));
}

TEST(ScanKernelTest, StringsCompareBytewise) {
  const std::vector<std::string> values{"b", "Zed", "abc", "b"};
  const std::span<const std::string> view(values);
  EXPECT_EQ(scanValues(view, CompareOp::Less, std::string("b")), (std::vector<RowId>{1, 2}));
  EXPECT_EQ(scanValues(view, CompareOp::Equal, std::string("b")), (std::vector<RowId>{0, 3}));
}

TEST(ScanKernelTest, EmptyInputSelectsNothing) {
  const std::vector<double> values;
  EXPECT_TRUE(scanValues(std::span<const double>(values), CompareOp::NotEqual, 1.0).empty());
  EXPECT_TRUE(allRows(0).empty());
}

}  // namespace
}  // namespace sillyql
