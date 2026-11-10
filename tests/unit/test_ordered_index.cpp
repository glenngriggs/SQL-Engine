#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "sillyql/ordered_index.hpp"

namespace sillyql {
namespace {

/// Scores 100, 200, 150, 200, 100, 150 at rows 0..5: three keys, each on two rows.
Column scoreColumn() {
  Column scores(ColumnType::Int);
  for (const std::int32_t score : {100, 200, 150, 200, 100, 150}) {
    scores.append(Value(score));
  }
  return scores;
}

TEST(OrderedIndexTest, DescribesItself) {
  const OrderedIndex<double> index(2);
  EXPECT_EQ(index.kind(), IndexKind::Ordered);
  EXPECT_EQ(index.name(), "bst");
  EXPECT_EQ(index.column(), 2U);
}

TEST(OrderedIndexTest, SupportsEverythingButNotEqual) {
  const OrderedIndex<std::int32_t> index(0);
  for (const CompareOp op : {CompareOp::Less, CompareOp::LessEqual, CompareOp::Equal,
                             CompareOp::GreaterEqual, CompareOp::Greater}) {
    EXPECT_TRUE(index.supports(op));
  }
  EXPECT_FALSE(index.supports(CompareOp::NotEqual));
  EXPECT_THROW((void)index.lookup(CompareOp::NotEqual, Value(1)), std::logic_error);
}

TEST(OrderedIndexTest, RangeLookupsReturnKeyOrderThenRowOrder) {
  OrderedIndex<std::int32_t> index(0);
  index.extend(scoreColumn(), 0);
  EXPECT_EQ(index.distinctKeys(), 3U);
  EXPECT_EQ(index.lookup(CompareOp::Greater, Value(100)), (std::vector<RowId>{2, 5, 1, 3}));
  EXPECT_EQ(index.lookup(CompareOp::GreaterEqual, Value(150)), (std::vector<RowId>{2, 5, 1, 3}));
  EXPECT_EQ(index.lookup(CompareOp::Less, Value(200)), (std::vector<RowId>{0, 4, 2, 5}));
  EXPECT_EQ(index.lookup(CompareOp::LessEqual, Value(150)), (std::vector<RowId>{0, 4, 2, 5}));
  EXPECT_EQ(index.lookup(CompareOp::Equal, Value(200)), (std::vector<RowId>{1, 3}));
}

TEST(OrderedIndexTest, BoundariesBetweenAndBeyondKeys) {
  OrderedIndex<std::int32_t> index(0);
  index.extend(scoreColumn(), 0);
  EXPECT_EQ(index.lookup(CompareOp::Greater, Value(175)), (std::vector<RowId>{1, 3}));
  EXPECT_EQ(index.lookup(CompareOp::LessEqual, Value(125)), (std::vector<RowId>{0, 4}));
  EXPECT_TRUE(index.lookup(CompareOp::Less, Value(100)).empty());
  EXPECT_TRUE(index.lookup(CompareOp::Greater, Value(200)).empty());
  EXPECT_TRUE(index.lookup(CompareOp::Equal, Value(125)).empty());
  EXPECT_EQ(index.lookup(CompareOp::GreaterEqual, Value(-5)).size(), 6U);
}

TEST(OrderedIndexTest, OrdersStringsBytewise) {
  Column labels(ColumnType::String);
  for (const char* label : {"one", "two", "three", "four", "five"}) {
    labels.append(Value(label));
  }
  OrderedIndex<std::string> index(0);
  index.extend(labels, 0);
  EXPECT_EQ(index.lookup(CompareOp::Less, Value("seven")), (std::vector<RowId>{4, 3, 0}));
  EXPECT_EQ(index.lookup(CompareOp::Greater, Value("four")), (std::vector<RowId>{0, 2, 1}));
}

TEST(OrderedIndexTest, OrdersBoolsFalseFirst) {
  Column flags(ColumnType::Bool);
  for (const bool flag : {true, false, true, false}) {
    flags.append(Value(flag));
  }
  OrderedIndex<BoolStorage> index(0);
  index.extend(flags, 0);
  EXPECT_EQ(index.lookup(CompareOp::GreaterEqual, Value(false)), (std::vector<RowId>{1, 3, 0, 2}));
  EXPECT_TRUE(index.lookup(CompareOp::Greater, Value(true)).empty());
}

TEST(OrderedIndexTest, OrdersDoublesAndKeepsRowOrderWithinAKey) {
  Column prices(ColumnType::Double);
  for (const double price : {2.5, 1.0, 2.5}) {
    prices.append(Value(price));
  }
  OrderedIndex<double> index(0);
  index.extend(prices, 0);
  EXPECT_EQ(index.lookup(CompareOp::GreaterEqual, Value(1.0)), (std::vector<RowId>{1, 0, 2}));
  EXPECT_EQ(index.lookup(CompareOp::Equal, Value(2.5)), (std::vector<RowId>{0, 2}));
}

}  // namespace
}  // namespace sillyql
