#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "sillyql/executor.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

using test::runScript;

const std::string kCars =
    "CREATE cars 3 string int double make miles price\n"
    "INSERT INTO cars 4 ROWS\n"
    "Ford 8437 63901.00\n"
    "Chevrolet 67226 25965.00\n"
    "Ford 43801 73429.00\n"
    "Subaru 15600 27441.50\n";

/// Output of `body` run after the cars fixture, without the fixture's own two lines.
std::string runOnCars(const std::string& body, SessionOptions options = {}) {
  const std::string out = runScript(kCars + body + "QUIT\n", options).out;
  const std::string fixture =
      "% New table cars with column(s) make miles price created\n"
      "% Added 4 rows to cars from position 0 to 3\n";
  EXPECT_EQ(out.substr(0, fixture.size()), fixture);
  return out.substr(fixture.size());
}

TEST(ExecutorTest, CreateListsColumnsInOrder) {
  EXPECT_EQ(runScript("CREATE t 2 int bool a b\nQUIT\n").out,
            "% New table t with column(s) a b created\n% Thanks for being silly!\n");
}

TEST(ExecutorTest, InsertReportsRowPositions) {
  EXPECT_EQ(runOnCars("INSERT INTO cars 2 ROWS\nKia 1 2\nKia 3 4\n"),
            "% Added 2 rows to cars from position 4 to 5\n% Thanks for being silly!\n");
  EXPECT_EQ(runOnCars("INSERT INTO cars 0 ROWS\n"),
            "% Added 0 rows to cars\n% Thanks for being silly!\n");
}

TEST(ExecutorTest, PrintAllUsesRowOrderAndTrailingSpaces) {
  EXPECT_EQ(runOnCars("PRINT FROM cars 2 price make ALL\n"),
            "% price make \n"
            "63901 Ford \n"
            "25965 Chevrolet \n"
            "73429 Ford \n"
            "27441.5 Subaru \n"
            "Printed 4 matching rows from cars\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, PrintWhereFiltersRows) {
  EXPECT_EQ(runOnCars("PRINT FROM cars 1 miles WHERE make = Ford\n"),
            "% miles \n8437 \n43801 \nPrinted 2 matching rows from cars\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, OrderedIndexRangePrintsInKeyOrder) {
  EXPECT_EQ(runOnCars("GENERATE FOR cars bst INDEX ON price\n"
                      "PRINT FROM cars 1 price WHERE price > 26000\n"),
            "% Generated bst index for table cars on column price, with 4 distinct keys\n"
            "% price \n27441.5 \n63901 \n73429 \nPrinted 3 matching rows from cars\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, QuietModeSuppressesOnlyRows) {
  EXPECT_EQ(runOnCars("PRINT FROM cars 1 make ALL\n"
                      "JOIN cars AND cars WHERE make = make AND PRINT 1 make 1\n"
                      "AGGREGATE FROM cars COUNT make\n",
                      {.quiet = true}),
            "% Printed 4 matching rows from cars\n"
            "% Printed 6 rows from joining cars to cars\n"
            "% Aggregated 4 rows from cars\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, DeleteRemovesMatchingRowsAndMaintainsIndex) {
  EXPECT_EQ(runOnCars("GENERATE FOR cars hash INDEX ON make\n"
                      "DELETE FROM cars WHERE miles < 20000\n"
                      "PRINT FROM cars 1 miles WHERE make = Ford\n"
                      "PRINT FROM cars 1 make WHERE make = Subaru\n"),
            "% Generated hash index for table cars on column make, with 3 distinct keys\n"
            "% Deleted 2 rows from cars\n"
            "% miles \n43801 \nPrinted 1 matching rows from cars\n"
            "% make \nPrinted 0 matching rows from cars\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, JoinPrintsColumnsFromTheRequestedSide) {
  const std::string out = runOnCars(
      "CREATE owners 2 string string name make\n"
      "INSERT INTO owners 3 ROWS\nann Ford\nbob Subaru\ncat Ford\n"
      "JOIN owners AND cars WHERE make = make AND PRINT 2 name 1 miles 2\n");
  EXPECT_NE(out.find("% name miles \n"
                     "ann 8437 \nann 43801 \nbob 15600 \ncat 8437 \ncat 43801 \n"
                     "Printed 5 rows from joining owners to cars\n"),
            std::string::npos)
      << out;
}

TEST(ExecutorTest, JoinOutputIsIndependentOfTheRightTableIndex) {
  const std::string join = "JOIN cars AND cars WHERE make = make AND PRINT 2 miles 1 miles 2\n";
  const std::string expected =
      "% miles miles \n"
      "8437 8437 \n8437 43801 \n67226 67226 \n43801 8437 \n43801 43801 \n15600 15600 \n"
      "Printed 6 rows from joining cars to cars\n% Thanks for being silly!\n";
  EXPECT_EQ(runOnCars(join), expected);
  for (const char* kind : {"hash", "bst"}) {
    const std::string generate = std::string("GENERATE FOR cars ") + kind + " INDEX ON make\n";
    const std::string out = runOnCars(generate + join);
    EXPECT_EQ(out.substr(out.find("% miles miles")), expected) << kind;
  }
}

TEST(ExecutorTest, RemoveDropsTheTable) {
  EXPECT_EQ(runOnCars("REMOVE cars\nPRINT FROM cars 1 make ALL\n"),
            "% Table cars removed\n"
            "% Error during PRINT: cars does not name a table in the database\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, AggregatePrintsHeaderGroupsAndSummary) {
  EXPECT_EQ(runOnCars("AGGREGATE FROM cars SUM miles GROUP BY make WHERE price > 26000\n"),
            "% make SUM(miles) \nFord 52238 \nSubaru 15600 \n"
            "Aggregated 3 rows from cars\n% Thanks for being silly!\n");
}

TEST(ExecutorTest, LoadAppendsCsvRows) {
  const std::string path = std::string(SILLYQL_TEST_DATA_DIR) + "/orders.csv";
  const std::string out = runScript(
                              "CREATE orders 4 int string double bool id trader notional urgent\n"
                              "LOAD INTO orders FROM " +
                              path + "\nQUIT\n")
                              .out;
  EXPECT_NE(out.find("% Loaded 5 rows into orders from position 0 to 4\n"), std::string::npos)
      << out;
}

TEST(ExecutorTest, LoadErrorLeavesTableUnchanged) {
  const std::string path = std::string(SILLYQL_TEST_DATA_DIR) + "/orders_bad_value.csv";
  const std::string out = runScript(
                              "CREATE orders 4 int string double bool id trader notional urgent\n"
                              "LOAD INTO orders FROM " +
                                  path + "\nPRINT FROM orders 1 id ALL\nQUIT\n",
                              {.quiet = true})
                              .out;
  EXPECT_NE(out.find("Error during LOAD: Line 3: 'lots' is not a valid double value for column "
                     "notional\n% Printed 0 matching rows from orders\n"),
            std::string::npos)
      << out;
}

TEST(ExecutorTest, ExplainDoesNotExecute) {
  EXPECT_EQ(runOnCars("EXPLAIN DELETE FROM cars WHERE miles > 0\nPRINT FROM cars 0 ALL\n",
                      {.quiet = true}),
            "% Plan: full scan of cars (filter: miles > 0)\n"
            "% Printed 4 matching rows from cars\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, CommentsPrintOnlyThePrompt) {
  EXPECT_EQ(runScript("# one\n#two\nQUIT\n").out, "% % % Thanks for being silly!\n");
}

TEST(ExecutorTest, AggregateLeavesATrailingCommentToTheNextCommand) {
  EXPECT_EQ(runOnCars("AGGREGATE FROM cars COUNT make # how many?\n", {.quiet = true}),
            "% Aggregated 4 rows from cars\n% % Thanks for being silly!\n");
}

TEST(ExecutorTest, ErrorDiscardsTheRestOfTheLineOnly) {
  EXPECT_EQ(runScript("BOGUS PRINT FROM x\nREMOVE ghost QUIT\nQUIT\n").out,
            "% Error: unrecognized command\n"
            "% Error during REMOVE: ghost does not name a table in the database\n"
            "% Thanks for being silly!\n");
}

TEST(ExecutorTest, ReturnsQuitOutcomeOnlyForQuit) {
  Catalog catalog;
  std::ostringstream out;
  ResultWriter writer(out, false);
  Executor executor(catalog, writer);
  EXPECT_EQ(executor.execute(CreateCmd{"t", {{"a", ColumnType::Int}}}), Outcome::Continue);
  EXPECT_EQ(executor.execute(CommentCmd{}), Outcome::Continue);
  EXPECT_EQ(executor.execute(QuitCmd{}), Outcome::Quit);
  EXPECT_EQ(out.str(), "New table t with column(s) a created\nThanks for being silly!\n");
  EXPECT_THROW((void)executor.execute(RemoveCmd{"ghost"}), TableNotFoundError);
}

}  // namespace
}  // namespace sillyql
