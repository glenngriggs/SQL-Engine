// Equi-join execution: both strategies (probing an existing index, building a temporary hash
// table) must produce exactly the pairs of a nested-loop reference join, in the same order.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "sillyql/join.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

using Pairs = std::vector<std::pair<RowId, RowId>>;

Pairs runJoin(const Table& left, std::size_t leftColumn, const Table& right,
              std::size_t rightColumn, JoinStrategy strategy) {
  Pairs pairs;
  const std::size_t count = executeJoin(
      left, leftColumn, right, rightColumn, strategy,
      [&pairs](RowId leftRow, RowId rightRow) { pairs.emplace_back(leftRow, rightRow); });
  EXPECT_EQ(count, pairs.size());
  return pairs;
}

/// Left rows in row order, each followed by its matching right rows in row order.
template <class Key>
Pairs nestedLoopJoin(const Table& left, std::size_t leftColumn, const Table& right,
                     std::size_t rightColumn) {
  const std::vector<Key>& probe = left.column(leftColumn).values<Key>();
  const std::vector<Key>& build = right.column(rightColumn).values<Key>();
  Pairs pairs;
  for (std::size_t l = 0; l < probe.size(); ++l) {
    for (std::size_t r = 0; r < build.size(); ++r) {
      if (probe[l] == build[r]) {
        pairs.emplace_back(static_cast<RowId>(l), static_cast<RowId>(r));
      }
    }
  }
  return pairs;
}

/// A table (k int, s string, b bool) of `rows` random rows drawn from few distinct keys, so
/// both sides of a join have many duplicates.
Table randomTable(const std::string& name, std::size_t rows, std::mt19937& rng) {
  Table table(name, {{"k", ColumnType::Int}, {"s", ColumnType::String}, {"b", ColumnType::Bool}});
  std::uniform_int_distribution<int> key(0, 6);
  RowBatch batch = table.emptyBatch();
  for (std::size_t row = 0; row < rows; ++row) {
    batch.columns[0].append(Value(static_cast<std::int32_t>(key(rng))));
    batch.columns[1].append(Value("s" + std::to_string(key(rng))));
    batch.columns[2].append(Value(key(rng) % 2 == 0));
  }
  batch.rows = rows;
  table.append(std::move(batch));
  return table;
}

/// Checks every strategy on `left.column = right.column` against the reference.
template <class Key>
void expectAllStrategiesAgree(const Table& left, Table& right, std::size_t column) {
  const Pairs expected = nestedLoopJoin<Key>(left, column, right, column);
  EXPECT_EQ(runJoin(left, column, right, column, JoinStrategy::BuildTemporaryHashTable), expected);
  for (const IndexKind kind : {IndexKind::Hash, IndexKind::Ordered}) {
    right.createIndex(kind, column);
    EXPECT_EQ(runJoin(left, column, right, column, JoinStrategy::ProbeExistingIndex), expected)
        << indexKindName(kind);
  }
}

class JoinDifferentialTest : public ::testing::TestWithParam<std::uint32_t> {};

TEST_P(JoinDifferentialTest, StrategiesMatchANestedLoopJoin) {
  std::mt19937 rng(GetParam());
  const Table left = randomTable("left", 1 + rng() % 40, rng);
  Table right = randomTable("right", rng() % 40, rng);
  expectAllStrategiesAgree<std::int32_t>(left, right, 0);
  expectAllStrategiesAgree<std::string>(left, right, 1);
  expectAllStrategiesAgree<BoolStorage>(left, right, 2);
}

TEST_P(JoinDifferentialTest, SelfJoinMatchesANestedLoopJoin) {
  std::mt19937 rng(GetParam());
  Table table = randomTable("t", 1 + rng() % 40, rng);
  const Pairs expected = nestedLoopJoin<std::string>(table, 1, table, 1);
  EXPECT_EQ(runJoin(table, 1, table, 1, JoinStrategy::BuildTemporaryHashTable), expected);
  table.createIndex(IndexKind::Hash, 1);
  EXPECT_EQ(runJoin(table, 1, table, 1, JoinStrategy::ProbeExistingIndex), expected);
}

INSTANTIATE_TEST_SUITE_P(FixedSeeds, JoinDifferentialTest, ::testing::Range<std::uint32_t>(1, 31));

TEST(JoinTest, EmptySidesProduceNoPairs) {
  const Table empty("e", {{"k", ColumnType::Int}});
  Table full("f", {{"k", ColumnType::Int}});
  full.append(test::batchOf(full, {{"1"}, {"2"}}));
  EXPECT_TRUE(runJoin(empty, 0, full, 0, JoinStrategy::BuildTemporaryHashTable).empty());
  EXPECT_TRUE(runJoin(full, 0, empty, 0, JoinStrategy::BuildTemporaryHashTable).empty());
}

TEST(JoinTest, DuplicateKeysOnBothSidesKeepRowOrder) {
  Table left("l", {{"k", ColumnType::Double}});
  left.append(test::batchOf(left, {{"2"}, {"1"}, {"2"}}));
  Table right("r", {{"k", ColumnType::Double}});
  right.append(test::batchOf(right, {{"2"}, {"3"}, {"2"}, {"-0"}}));
  const Pairs expected{{0, 0}, {0, 2}, {2, 0}, {2, 2}};
  EXPECT_EQ(runJoin(left, 0, right, 0, JoinStrategy::BuildTemporaryHashTable), expected);
  right.createIndex(IndexKind::Hash, 0);
  EXPECT_EQ(runJoin(left, 0, right, 0, JoinStrategy::ProbeExistingIndex), expected);
}

}  // namespace
}  // namespace sillyql
