#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "sillyql/table.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

using test::batchOf;
using test::columnText;

Table makeCities() {
  Table table("cities", {{"name", ColumnType::String},
                         {"population", ColumnType::Int},
                         {"capital", ColumnType::Bool}});
  table.append(batchOf(table, {{"Lansing", "116020", "true"},
                               {"Detroit", "639111", "false"},
                               {"Albany", "98111", "true"}}));
  return table;
}

TEST(TableTest, ExposesSchema) {
  const Table table = makeCities();
  EXPECT_EQ(table.name(), "cities");
  EXPECT_EQ(table.columnCount(), 3U);
  EXPECT_EQ(table.rowCount(), 3U);
  EXPECT_EQ(table.spec(1), (ColumnSpec{"population", ColumnType::Int}));
  EXPECT_EQ(table.findColumn("capital"), 2U);
  EXPECT_FALSE(table.findColumn("mayor"));
}

TEST(TableTest, RejectsDuplicateColumnNames) {
  EXPECT_THROW(Table("t", {{"a", ColumnType::Int}, {"a", ColumnType::Bool}}),
               std::invalid_argument);
}

TEST(TableTest, AppendReturnsFirstNewRowId) {
  Table table = makeCities();
  const RowId first = table.append(batchOf(table, {{"Austin", "961855", "true"}}));
  EXPECT_EQ(first, 3U);
  EXPECT_EQ(table.rowCount(), 4U);
  EXPECT_EQ(table.column(0).valueAt(3), Value("Austin"));
}

TEST(TableTest, AppendRejectsMismatchedBatch) {
  Table table = makeCities();
  RowBatch wrongWidth;
  wrongWidth.columns.emplace_back(ColumnType::String);
  EXPECT_THROW(table.append(std::move(wrongWidth)), std::invalid_argument);

  RowBatch wrongCount = table.emptyBatch();
  wrongCount.rows = 1;  // Claims a row its columns do not have.
  EXPECT_THROW(table.append(std::move(wrongCount)), std::invalid_argument);
  EXPECT_EQ(table.rowCount(), 3U);
}

TEST(TableTest, EraseCompactsEveryColumnAndIgnoresDuplicates) {
  Table table = makeCities();
  const std::vector<RowId> rows{0, 2, 0};
  EXPECT_EQ(table.erase(rows), 2U);
  EXPECT_EQ(table.rowCount(), 1U);
  EXPECT_EQ(columnText(table, 0, {0}), (std::vector<std::string>{"Detroit"}));
  EXPECT_EQ(columnText(table, 1, {0}), (std::vector<std::string>{"639111"}));
  EXPECT_EQ(table.erase({}), 0U);
}

TEST(TableTest, CreateIndexReplacesThePreviousIndex) {
  Table table = makeCities();
  EXPECT_EQ(table.indexOn(2), nullptr);
  EXPECT_EQ(table.createIndex(IndexKind::Hash, 2), 2U);
  ASSERT_NE(table.indexOn(2), nullptr);
  EXPECT_EQ(table.indexOn(2)->kind(), IndexKind::Hash);

  EXPECT_EQ(table.createIndex(IndexKind::Ordered, 0), 3U);
  EXPECT_EQ(table.indexOn(2), nullptr);
  ASSERT_NE(table.indexOn(0), nullptr);
  EXPECT_EQ(table.indexOn(0)->kind(), IndexKind::Ordered);
}

TEST(TableTest, AppendExtendsTheIndex) {
  Table table = makeCities();
  table.createIndex(IndexKind::Hash, 2);
  table.append(batchOf(table, {{"Austin", "961855", "true"}}));
  EXPECT_EQ(table.indexOn(2)->lookup(CompareOp::Equal, Value(true)), (std::vector<RowId>{0, 2, 3}));
}

TEST(TableTest, RepeatedAppendsKeepEveryColumnAligned) {
  Table table = makeCities();
  for (int i = 0; i < 50; ++i) {
    table.append(batchOf(table, {{"Town" + std::to_string(i), std::to_string(i), "false"}}));
  }
  EXPECT_EQ(table.rowCount(), 53U);
  for (std::size_t column = 0; column < table.columnCount(); ++column) {
    EXPECT_EQ(table.column(column).size(), 53U);
  }
  EXPECT_EQ(columnText(table, 0, {52}), (std::vector<std::string>{"Town49"}));
}

TEST(TableTest, EraseRenumbersTheIndex) {
  Table table = makeCities();
  table.createIndex(IndexKind::Ordered, 0);
  const std::vector<RowId> first{0};
  table.erase(first);
  const Index& index = *table.indexOn(0);
  EXPECT_EQ(index.lookup(CompareOp::Equal, Value("Albany")), (std::vector<RowId>{1}));
  EXPECT_TRUE(index.lookup(CompareOp::Equal, Value("Lansing")).empty());
  EXPECT_EQ(index.distinctKeys(), 2U);
  EXPECT_EQ(index.lookup(CompareOp::Greater, Value("A")), (std::vector<RowId>{1, 0}));
}

TEST(TableTest, EraseOfUnknownRowThrowsBeforeChangingAnything) {
  Table table = makeCities();
  const std::vector<RowId> rows{0, 7};
  EXPECT_THROW(table.erase(rows), std::out_of_range);
  EXPECT_EQ(table.rowCount(), 3U);
}

}  // namespace
}  // namespace sillyql
