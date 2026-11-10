#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "sillyql/csv_loader.hpp"
#include "sillyql/errors.hpp"

namespace sillyql {
namespace {

using Fields = std::vector<std::string>;

class CsvLoaderTest : public ::testing::Test {
 protected:
  [[nodiscard]] RowBatch load(const std::string& csv) const {
    std::istringstream in(csv);
    return readCsv(in, table_);
  }

  [[nodiscard]] std::string errorOf(const std::string& csv) const {
    try {
      (void)load(csv);
    } catch (const LoadError& error) {
      return error.what();
    }
    return "<no error>";
  }

  Table table_{
      "orders",
      {{"id", ColumnType::Int}, {"trader", ColumnType::String}, {"px", ColumnType::Double}}};
};

TEST(SplitCsvRecordTest, SplitsPlainFields) {
  EXPECT_EQ(splitCsvRecord("a,b,c"), (Fields{"a", "b", "c"}));
  EXPECT_EQ(splitCsvRecord("one"), (Fields{"one"}));
  EXPECT_EQ(splitCsvRecord("a,,c"), (Fields{"a", "", "c"}));
  EXPECT_EQ(splitCsvRecord("a,"), (Fields{"a", ""}));
}

TEST(SplitCsvRecordTest, TrimsBlanksAroundFields) {
  EXPECT_EQ(splitCsvRecord("  a ,\tb\t, c"), (Fields{"a", "b", "c"}));
}

TEST(SplitCsvRecordTest, HandlesQuotedFields) {
  EXPECT_EQ(splitCsvRecord(R"("a,b",c)"), (Fields{"a,b", "c"}));
  EXPECT_EQ(splitCsvRecord(R"("say ""hi""", x)"), (Fields{R"(say "hi")", "x"}));
  EXPECT_EQ(splitCsvRecord(R"( "padded" ,"")"), (Fields{"padded", ""}));
}

TEST(SplitCsvRecordTest, RejectsMalformedQuoting) {
  EXPECT_FALSE(splitCsvRecord(R"("open,field)"));
  EXPECT_FALSE(splitCsvRecord(R"("closed"junk,x)"));
}

TEST_F(CsvLoaderTest, BindsHeaderColumnsInAnyOrder) {
  const RowBatch batch = load("px,id,trader\n1.5,7,kim\n2,8,lee\n");
  ASSERT_EQ(batch.rows, 2U);
  EXPECT_EQ(batch.columns[0].values<std::int32_t>(), (std::vector<std::int32_t>{7, 8}));
  EXPECT_EQ(batch.columns[1].values<std::string>(), (std::vector<std::string>{"kim", "lee"}));
  EXPECT_EQ(batch.columns[2].values<double>(), (std::vector<double>{1.5, 2.0}));
}

TEST_F(CsvLoaderTest, SkipsBlankLinesAndAcceptsCrlf) {
  const RowBatch batch = load("\r\nid,trader,px\r\n\r\n1,a,1\r\n   \n2,b,2\r\n");
  EXPECT_EQ(batch.rows, 2U);
  EXPECT_EQ(batch.columns[1].values<std::string>(), (std::vector<std::string>{"a", "b"}));
}

TEST_F(CsvLoaderTest, HeaderOnlyGivesEmptyBatch) {
  const RowBatch batch = load("id,trader,px\n");
  EXPECT_EQ(batch.rows, 0U);
  EXPECT_EQ(batch.columns.size(), 3U);
}

TEST_F(CsvLoaderTest, ReportsHeaderProblems) {
  EXPECT_EQ(errorOf(""), "Error during LOAD: File has no header row");
  EXPECT_EQ(errorOf("id,trader,px,venue\n"),
            "Error during LOAD: Header column venue does not name a column in orders");
  EXPECT_EQ(errorOf("id,trader,id\n"), "Error during LOAD: Header lists column id more than once");
  EXPECT_EQ(errorOf("id,trader\n"), "Error during LOAD: Header does not list column px of orders");
}

TEST_F(CsvLoaderTest, ReportsRowProblemsWithLineNumbers) {
  EXPECT_EQ(errorOf("id,trader,px\n1,a,1\n\nx,b,2\n"),
            "Error during LOAD: Line 4: 'x' is not a valid int value for column id");
  EXPECT_EQ(errorOf("id,trader,px\n1,a\n"), "Error during LOAD: Line 2 has 2 fields, expected 3");
  EXPECT_EQ(errorOf("id,trader,px\n1,\"a,1\n"),
            "Error during LOAD: Line 2: malformed quoted field");
}

TEST_F(CsvLoaderTest, StringValuesMustBeSingleTokens) {
  EXPECT_EQ(errorOf("id,trader,px\n1,\"kim park\",1\n"),
            "Error during LOAD: Line 2: 'kim park' is not a valid string value for column trader");
  EXPECT_EQ(errorOf("id,trader,px\n1,,1\n"),
            "Error during LOAD: Line 2: '' is not a valid string value for column trader");
  EXPECT_EQ(errorOf("id,trader,px\n,kim,1\n"),
            "Error during LOAD: Line 2: '' is not a valid int value for column id");
  EXPECT_EQ(load("id,trader,px\n1,\"a,b\",1\n").columns[1].values<std::string>(),
            (std::vector<std::string>{"a,b"}));
}

TEST_F(CsvLoaderTest, ReadsFilesAndReportsMissingOnes) {
  const RowBatch batch = readCsvFile(std::string(SILLYQL_TEST_DATA_DIR) + "/orders.csv",
                                     Table("orders", {{"id", ColumnType::Int},
                                                      {"trader", ColumnType::String},
                                                      {"notional", ColumnType::Double},
                                                      {"urgent", ColumnType::Bool}}));
  EXPECT_EQ(batch.rows, 5U);
  EXPECT_EQ(batch.columns[1].values<std::string>(),
            (std::vector<std::string>{"smith", "lee", "jones", "o\"brien", "lee"}));
  EXPECT_THROW((void)readCsvFile("/no/such/file.csv", table_), LoadError);
  try {
    (void)readCsvFile(SILLYQL_TEST_DATA_DIR, table_);
    FAIL() << "expected LoadError";
  } catch (const LoadError& error) {
    EXPECT_EQ(std::string(error.what()),
              std::string("Error during LOAD: Cannot open file ") + SILLYQL_TEST_DATA_DIR);
  }
}

}  // namespace
}  // namespace sillyql
