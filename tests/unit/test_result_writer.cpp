#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>

#include "sillyql/result_writer.hpp"

namespace sillyql {
namespace {

/// Writes one header and one data row covering every kind of cell.
std::string writeSample(bool quiet) {
  std::ostringstream out;
  ResultWriter writer(out, quiet);
  writer.prompt();
  writer.cell("price");
  writer.cell("flag");
  writer.endRow();
  Column prices(ColumnType::Double);
  prices.append(Value(25965.0));
  writer.cell(prices, 0);
  writer.cell(Value(true));
  writer.cell(AggregateValue(std::int64_t{4000000000}));
  writer.cell(AggregateValue(std::monostate{}));
  writer.endRow();
  writer.line("Printed ", 1, " matching rows from t");
  return out.str();
}

TEST(ResultWriterTest, EveryCellIsFollowedByOneSpace) {
  EXPECT_EQ(writeSample(false),
            "% price flag \n25965 true 4000000000 NULL \nPrinted 1 matching rows from t\n");
}

TEST(ResultWriterTest, QuietModeDropsRowsButKeepsPromptsAndSummaries) {
  EXPECT_EQ(writeSample(true), "% Printed 1 matching rows from t\n");
}

TEST(ResultWriterTest, ShowRowsReflectsQuietMode) {
  std::ostringstream out;
  EXPECT_TRUE(ResultWriter(out, false).showRows());
  EXPECT_FALSE(ResultWriter(out, true).showRows());
}

}  // namespace
}  // namespace sillyql
