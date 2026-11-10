#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "sillyql/hash_index.hpp"

namespace sillyql {
namespace {

Column stringColumn(const std::vector<std::string>& values) {
  Column column(ColumnType::String);
  for (const std::string& value : values) {
    column.append(Value(value));
  }
  return column;
}

TEST(HashIndexTest, DescribesItself) {
  const HashIndex<std::int32_t> index(3);
  EXPECT_EQ(index.kind(), IndexKind::Hash);
  EXPECT_EQ(index.name(), "hash");
  EXPECT_EQ(index.column(), 3U);
  EXPECT_EQ(index.distinctKeys(), 0U);
}

TEST(HashIndexTest, SupportsOnlyEquality) {
  const HashIndex<std::int32_t> index(0);
  EXPECT_TRUE(index.supports(CompareOp::Equal));
  for (const CompareOp op : {CompareOp::Less, CompareOp::LessEqual, CompareOp::NotEqual,
                             CompareOp::GreaterEqual, CompareOp::Greater}) {
    EXPECT_FALSE(index.supports(op));
    EXPECT_THROW((void)index.lookup(op, Value(1)), std::logic_error);
  }
}

TEST(HashIndexTest, EqualityLookupReturnsRowsInRowOrder) {
  HashIndex<std::string> index(0);
  index.extend(stringColumn({"happy", "sad", "happy", "busy", "happy"}), 0);
  EXPECT_EQ(index.distinctKeys(), 3U);
  EXPECT_EQ(index.lookup(CompareOp::Equal, Value("happy")), (std::vector<RowId>{0, 2, 4}));
  EXPECT_EQ(index.lookup(CompareOp::Equal, Value("sad")), (std::vector<RowId>{1}));
  EXPECT_TRUE(index.lookup(CompareOp::Equal, Value("angry")).empty());
}

TEST(HashIndexTest, HandlesBoolAndDoubleKeys) {
  HashIndex<BoolStorage> flags(0);
  Column bools(ColumnType::Bool);
  for (const bool value : {true, false, true}) {
    bools.append(Value(value));
  }
  flags.extend(bools, 0);
  EXPECT_EQ(flags.lookup(CompareOp::Equal, Value(true)), (std::vector<RowId>{0, 2}));

  HashIndex<double> prices(0);
  Column zero(ColumnType::Double);
  zero.append(Value(0.0));
  prices.extend(zero, 0);
  EXPECT_EQ(prices.lookup(CompareOp::Equal, Value(-0.0)), (std::vector<RowId>{0}));
}

TEST(HashIndexTest, RejectsKeysOfTheWrongType) {
  const HashIndex<std::int32_t> index(0);
  EXPECT_THROW((void)index.lookup(CompareOp::Equal, Value(7.0)), TypeMismatchError);
}

TEST(MakeIndexTest, CreatesTheRequestedKindForEveryType) {
  for (const ColumnType type :
       {ColumnType::Int, ColumnType::Double, ColumnType::Bool, ColumnType::String}) {
    EXPECT_EQ(makeIndex(IndexKind::Hash, type, 1)->kind(), IndexKind::Hash);
    EXPECT_EQ(makeIndex(IndexKind::Ordered, type, 1)->kind(), IndexKind::Ordered);
  }
  EXPECT_EQ(parseIndexKind("hash"), IndexKind::Hash);
  EXPECT_EQ(parseIndexKind("bst"), IndexKind::Ordered);
  EXPECT_FALSE(parseIndexKind("btree"));
  EXPECT_EQ(indexKindName(IndexKind::Ordered), "bst");
}

}  // namespace
}  // namespace sillyql
