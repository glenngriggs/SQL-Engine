#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "sillyql/planner.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

using test::batchOf;
using test::columnText;

class PlannerTest : public ::testing::Test {
 protected:
  PlannerTest() {
    trades_.append(batchOf(trades_, {{"1", "AAPL", "190.5"},
                                     {"2", "MSFT", "420.25"},
                                     {"3", "AAPL", "189.75"},
                                     {"4", "NVDA", "131.5"},
                                     {"5", "MSFT", "419.5"}}));
  }

  /// The access path chosen for `column op value`.
  [[nodiscard]] AccessPath pathFor(std::size_t column, CompareOp op, Value value) const {
    const Predicate where{column, op, std::move(value)};
    return planScan(trades_, &where).path;
  }

  /// EXPLAIN's description of `column op value`.
  [[nodiscard]] std::string describeFor(std::size_t column, CompareOp op, Value value) const {
    const Predicate where{column, op, std::move(value)};
    return describe(trades_, planScan(trades_, &where));
  }

  /// The ids (column 0) of the rows selected for `where`, in output order.
  [[nodiscard]] std::vector<std::string> select(const Predicate& where) const {
    return columnText(trades_, kId, executeScan(trades_, planScan(trades_, &where)));
  }

  static constexpr std::size_t kId = 0;
  static constexpr std::size_t kSymbol = 1;
  static constexpr std::size_t kPrice = 2;

  Table trades_{
      "trades",
      {{"id", ColumnType::Int}, {"symbol", ColumnType::String}, {"price", ColumnType::Double}}};
};

TEST_F(PlannerTest, WithoutIndexEveryPredicateScans) {
  for (const CompareOp op : {CompareOp::Less, CompareOp::Equal, CompareOp::NotEqual}) {
    EXPECT_EQ(pathFor(kId, op, Value(3)), AccessPath::FullScan);
  }
  EXPECT_EQ(planScan(trades_, nullptr).path, AccessPath::FullScan);
}

TEST_F(PlannerTest, HashIndexServesOnlyEqualityOnItsColumn) {
  trades_.createIndex(IndexKind::Hash, kSymbol);
  EXPECT_EQ(pathFor(kSymbol, CompareOp::Equal, Value("AAPL")), AccessPath::HashProbe);
  EXPECT_EQ(pathFor(kSymbol, CompareOp::Less, Value("B")), AccessPath::FullScan);
  EXPECT_EQ(pathFor(kSymbol, CompareOp::NotEqual, Value("B")), AccessPath::FullScan);
  EXPECT_EQ(pathFor(kId, CompareOp::Equal, Value(1)), AccessPath::FullScan);
}

TEST_F(PlannerTest, OrderedIndexServesEqualityAndRanges) {
  trades_.createIndex(IndexKind::Ordered, kPrice);
  EXPECT_EQ(pathFor(kPrice, CompareOp::Equal, Value(1.0)), AccessPath::OrderedPointLookup);
  for (const CompareOp op :
       {CompareOp::Less, CompareOp::LessEqual, CompareOp::GreaterEqual, CompareOp::Greater}) {
    EXPECT_EQ(pathFor(kPrice, op, Value(1.0)), AccessPath::OrderedRangeScan);
  }
  EXPECT_EQ(pathFor(kPrice, CompareOp::NotEqual, Value(1.0)), AccessPath::FullScan);
}

TEST_F(PlannerTest, ScansReturnRowOrderAndRangeScansReturnKeyOrder) {
  const Predicate cheap{kPrice, CompareOp::LessEqual, Value(400.0)};
  EXPECT_EQ(select(cheap), (std::vector<std::string>{"1", "3", "4"}));
  trades_.createIndex(IndexKind::Ordered, kPrice);
  EXPECT_EQ(select(cheap), (std::vector<std::string>{"4", "3", "1"}));
  EXPECT_EQ(select({kPrice, CompareOp::NotEqual, Value(131.5)}),
            (std::vector<std::string>{"1", "2", "3", "5"}));
}

TEST_F(PlannerTest, EveryAccessPathSelectsTheSameRows) {
  const Predicate msft{kSymbol, CompareOp::Equal, Value("MSFT")};
  const std::vector<RowId> scanned = executeScan(trades_, planScan(trades_, &msft));
  trades_.createIndex(IndexKind::Hash, kSymbol);
  EXPECT_EQ(executeScan(trades_, planScan(trades_, &msft)), scanned);
  trades_.createIndex(IndexKind::Ordered, kSymbol);
  EXPECT_EQ(executeScan(trades_, planScan(trades_, &msft)), scanned);
  EXPECT_EQ(scanned, (std::vector<RowId>{1, 4}));
}

TEST_F(PlannerTest, NoPredicateSelectsEveryRow) {
  EXPECT_EQ(executeScan(trades_, planScan(trades_, nullptr)), (std::vector<RowId>{0, 1, 2, 3, 4}));
}

TEST_F(PlannerTest, DescribesScanPlans) {
  EXPECT_EQ(describe(trades_, planScan(trades_, nullptr)), "Plan: full scan of trades (no filter)");
  EXPECT_EQ(describeFor(kPrice, CompareOp::Greater, Value(100.5)),
            "Plan: full scan of trades (filter: price > 100.5)");
  trades_.createIndex(IndexKind::Hash, kSymbol);
  EXPECT_EQ(describeFor(kSymbol, CompareOp::Equal, Value("A")),
            "Plan: hash-index probe on symbol (=)");
  trades_.createIndex(IndexKind::Ordered, kPrice);
  EXPECT_EQ(describeFor(kPrice, CompareOp::Equal, Value(1.0)),
            "Plan: ordered-index point lookup on price (=)");
  EXPECT_EQ(describeFor(kPrice, CompareOp::Less, Value(1.0)),
            "Plan: ordered-index range scan on price (<)");
}

TEST_F(PlannerTest, JoinProbesOnlyAnExistingHashIndexOnTheRightColumn) {
  EXPECT_EQ(planJoin(trades_, kSymbol), JoinStrategy::BuildTemporaryHashTable);
  EXPECT_EQ(describeJoin(trades_, kSymbol, JoinStrategy::BuildTemporaryHashTable),
            "Plan: hash join building temporary hash table on trades.symbol");
  trades_.createIndex(IndexKind::Hash, kSymbol);
  EXPECT_EQ(planJoin(trades_, kSymbol), JoinStrategy::ProbeExistingIndex);
  EXPECT_EQ(planJoin(trades_, kId), JoinStrategy::BuildTemporaryHashTable);
  EXPECT_EQ(describeJoin(trades_, kSymbol, JoinStrategy::ProbeExistingIndex),
            "Plan: hash join probing existing hash index on trades.symbol");
  trades_.createIndex(IndexKind::Ordered, kSymbol);
  EXPECT_EQ(planJoin(trades_, kSymbol), JoinStrategy::BuildTemporaryHashTable);
  EXPECT_EQ(describeJoin(trades_, kSymbol, JoinStrategy::BuildTemporaryHashTable),
            "Plan: hash join building temporary hash table on trades.symbol (existing bst index "
            "not probed)");
}

}  // namespace
}  // namespace sillyql
