#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "sillyql/aggregate.hpp"
#include "sillyql/scan.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

using test::batchOf;

class AggregateTest : public ::testing::Test {
 protected:
  AggregateTest() {
    fills_.append(batchOf(fills_, {{"AAPL", "BUY", "100", "190.25", "true"},
                                   {"AAPL", "SELL", "200", "190.5", "false"},
                                   {"MSFT", "BUY", "300", "420.1", "true"},
                                   {"NVDA", "BUY", "2000000000", "131.5", "false"},
                                   {"NVDA", "BUY", "2000000000", "131.25", "true"},
                                   {"GOOG", "SELL", "400", "171.3", "false"}}));
  }

  [[nodiscard]] AggregateResult run(AggregateFunction function, std::size_t column,
                                    std::optional<std::size_t> groupBy = std::nullopt) const {
    return aggregate(fills_, {function, column, groupBy}, allRows(fills_.rowCount()));
  }

  static constexpr std::size_t kSymbol = 0;
  static constexpr std::size_t kSide = 1;
  static constexpr std::size_t kQty = 2;
  static constexpr std::size_t kPrice = 3;
  static constexpr std::size_t kMaker = 4;

  Table fills_{"fills",
               {{"symbol", ColumnType::String},
                {"side", ColumnType::String},
                {"qty", ColumnType::Int},
                {"price", ColumnType::Double},
                {"maker", ColumnType::Bool}}};
};

std::string text(const AggregateValue& value) {
  std::ostringstream os;
  writeAggregateValue(os, value);
  return os.str();
}

TEST_F(AggregateTest, CountsRows) {
  const AggregateResult result = run(AggregateFunction::Count, kSymbol);
  ASSERT_EQ(result.groups.size(), 1U);
  EXPECT_FALSE(result.groups[0].key);
  EXPECT_EQ(result.groups[0].value, AggregateValue(std::int64_t{6}));
  EXPECT_EQ(result.inputRows, 6U);
}

TEST_F(AggregateTest, IntSumIsExactBeyondInt32) {
  const AggregateResult result = run(AggregateFunction::Sum, kQty);
  EXPECT_EQ(result.groups[0].value, AggregateValue(std::int64_t{4'000'001'000}));
}

TEST_F(AggregateTest, AverageOfIntsIsADouble) {
  const AggregateResult result = run(AggregateFunction::Avg, kQty, kSide);
  ASSERT_EQ(result.groups.size(), 2U);
  EXPECT_EQ(result.groups[1].key, Value("SELL"));
  EXPECT_EQ(result.groups[1].value, AggregateValue(300.0));
}

TEST_F(AggregateTest, GroupsAppearInAscendingKeyOrder) {
  const AggregateResult result = run(AggregateFunction::Avg, kPrice, kSymbol);
  ASSERT_EQ(result.groups.size(), 4U);
  EXPECT_EQ(result.groups[0].key, Value("AAPL"));
  EXPECT_EQ(result.groups[1].key, Value("GOOG"));
  EXPECT_EQ(result.groups[2].key, Value("MSFT"));
  EXPECT_EQ(result.groups[3].key, Value("NVDA"));
  EXPECT_EQ(text(result.groups[0].value), "190.375");
  EXPECT_EQ(text(result.groups[3].value), "131.375");
}

TEST_F(AggregateTest, GroupsByBoolAndIntKeys) {
  const AggregateResult byMaker = run(AggregateFunction::Count, kSymbol, kMaker);
  ASSERT_EQ(byMaker.groups.size(), 2U);
  EXPECT_EQ(byMaker.groups[0].key, Value(false));
  EXPECT_EQ(byMaker.groups[0].value, AggregateValue(std::int64_t{3}));

  const AggregateResult byQty = run(AggregateFunction::Max, kPrice, kQty);
  ASSERT_EQ(byQty.groups.size(), 5U);
  EXPECT_EQ(byQty.groups[0].key, Value(100));
  EXPECT_EQ(byQty.groups[3].key, Value(400));
  EXPECT_EQ(byQty.groups[4].key, Value(2'000'000'000));
  EXPECT_EQ(byQty.groups[4].value, AggregateValue(Value(131.5)));
}

TEST_F(AggregateTest, MinAndMaxWorkOnEveryType) {
  EXPECT_EQ(run(AggregateFunction::Min, kSymbol).groups[0].value, AggregateValue(Value("AAPL")));
  EXPECT_EQ(run(AggregateFunction::Max, kSymbol).groups[0].value, AggregateValue(Value("NVDA")));
  EXPECT_EQ(run(AggregateFunction::Min, kMaker).groups[0].value, AggregateValue(Value(false)));
  EXPECT_EQ(run(AggregateFunction::Max, kPrice).groups[0].value, AggregateValue(Value(420.1)));
  EXPECT_EQ(run(AggregateFunction::Min, kQty).groups[0].value, AggregateValue(Value(100)));
}

TEST_F(AggregateTest, EmptyInputWithoutGroupByYieldsZeroCountOrNull) {
  for (const AggregateFunction function : {AggregateFunction::Sum, AggregateFunction::Avg,
                                           AggregateFunction::Min, AggregateFunction::Max}) {
    const AggregateResult result = aggregate(fills_, {function, kQty, std::nullopt}, {});
    ASSERT_EQ(result.groups.size(), 1U);
    EXPECT_EQ(text(result.groups[0].value), "NULL");
  }
  const AggregateResult count =
      aggregate(fills_, {AggregateFunction::Count, kQty, std::nullopt}, {});
  EXPECT_EQ(text(count.groups[0].value), "0");
  EXPECT_EQ(count.inputRows, 0U);
}

TEST_F(AggregateTest, EmptyInputWithGroupByYieldsNoGroups) {
  EXPECT_TRUE(aggregate(fills_, {AggregateFunction::Sum, kQty, kSide}, {}).groups.empty());
}

TEST_F(AggregateTest, ResultDoesNotDependOnRowOrder) {
  std::vector<RowId> rows = allRows(fills_.rowCount());
  const AggregateResult forward = aggregate(fills_, {AggregateFunction::Sum, kPrice, kSide}, rows);
  std::reverse(rows.begin(), rows.end());
  const AggregateResult backward = aggregate(fills_, {AggregateFunction::Sum, kPrice, kSide}, rows);
  ASSERT_EQ(forward.groups.size(), backward.groups.size());
  for (std::size_t i = 0; i < forward.groups.size(); ++i) {
    EXPECT_EQ(forward.groups[i].value, backward.groups[i].value);
  }
}

TEST(AggregateFunctionTest, ParsesNamesAndChecksTypes) {
  for (const AggregateFunction function :
       {AggregateFunction::Count, AggregateFunction::Sum, AggregateFunction::Avg,
        AggregateFunction::Min, AggregateFunction::Max}) {
    EXPECT_EQ(parseAggregateFunction(aggregateFunctionName(function)), function);
    EXPECT_TRUE(acceptsColumnType(function, ColumnType::Int));
    EXPECT_TRUE(acceptsColumnType(function, ColumnType::Double));
  }
  EXPECT_FALSE(parseAggregateFunction("count"));
  EXPECT_FALSE(acceptsColumnType(AggregateFunction::Sum, ColumnType::String));
  EXPECT_FALSE(acceptsColumnType(AggregateFunction::Avg, ColumnType::Bool));
  EXPECT_TRUE(acceptsColumnType(AggregateFunction::Max, ColumnType::String));
  EXPECT_TRUE(acceptsColumnType(AggregateFunction::Count, ColumnType::Bool));
}

TEST(AggregateValueTest, WritesEachAlternative) {
  EXPECT_EQ(text(std::monostate{}), "NULL");
  EXPECT_EQ(text(std::int64_t{4'000'000'000}), "4000000000");
  EXPECT_EQ(text(2.5), "2.5");
  EXPECT_EQ(text(Value(true)), "true");
}

TEST(CompensatedSumTest, IsAccurateWhereNaiveSummationDrifts) {
  constexpr int kTerms = 10'000'000;
  CompensatedSum compensated;
  double naive = 0.0;
  for (int i = 0; i < kTerms; ++i) {
    compensated.add(0.1);
    naive += 0.1;
  }
  const double exact = 1'000'000.0;
  EXPECT_LT(std::abs(compensated.value() - exact), 1e-9);
  EXPECT_GT(std::abs(naive - exact), 1e-6);
}

TEST(CompensatedSumTest, RecoversSmallTermsNextToLargeOnes) {
  CompensatedSum sum;
  for (const double term : {1e16, 1.0, -1e16, 1.0}) {
    sum.add(term);
  }
  EXPECT_EQ(sum.value(), 2.0);
}

TEST(CompensatedSumTest, OverflowGivesInfinityNotNan) {
  CompensatedSum sum;
  sum.add(std::numeric_limits<double>::max());
  sum.add(std::numeric_limits<double>::max());
  EXPECT_TRUE(std::isinf(sum.value()));
}

}  // namespace
}  // namespace sillyql
