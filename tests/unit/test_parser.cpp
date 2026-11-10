#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <variant>

#include "sillyql/errors.hpp"
#include "sillyql/parser.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

class ParserTest : public ::testing::Test {
 protected:
  ParserTest() {
    Table& trades = catalog_.create("trades", {{"id", ColumnType::Int},
                                               {"symbol", ColumnType::String},
                                               {"price", ColumnType::Double},
                                               {"buy", ColumnType::Bool}});
    trades.append(test::batchOf(trades, {{"1", "AAPL", "190.5", "true"}}));
    catalog_.create("venues", {{"symbol", ColumnType::String}, {"mic", ColumnType::String}});
  }

  /// Parses the first command of `text`.
  Command parse(const std::string& text) {
    input_.str(text);
    input_.clear();
    return parser_.parse(tokens_.next());
  }

  template <class T>
  T parseAs(const std::string& text) {
    Command command = parse(text);
    EXPECT_TRUE(std::holds_alternative<T>(command)) << "parsed as alternative " << command.index();
    return std::get<T>(std::move(command));
  }

  /// The SqlError message produced by parsing `text`.
  std::string errorOf(const std::string& text) {
    try {
      (void)parse(text);
    } catch (const SqlError& error) {
      return error.what();
    }
    return "<no error>";
  }

  /// The token that follows the point where parsing stopped.
  std::string nextToken() { return tokens_.next(); }

  Catalog catalog_;
  std::istringstream input_;
  TokenStream tokens_{input_};
  Parser parser_{tokens_, catalog_};
};

TEST_F(ParserTest, CommentSkipsTheRestOfTheLine) {
  parseAs<CommentCmd>("#CREATE t 1 int a\nQUIT\n");
  EXPECT_EQ(nextToken(), "QUIT");
  parseAs<CommentCmd>("# spaced comment\n");
}

TEST_F(ParserTest, ParsesCreate) {
  const auto cmd = parseAs<CreateCmd>("CREATE pets 3 string bool double name cats weight\n");
  EXPECT_EQ(cmd.table, "pets");
  EXPECT_EQ(cmd.columns, (std::vector<ColumnSpec>{{"name", ColumnType::String},
                                                  {"cats", ColumnType::Bool},
                                                  {"weight", ColumnType::Double}}));
}

TEST_F(ParserTest, CreateErrors) {
  EXPECT_EQ(errorOf("CREATE trades 1 int a\n"),
            "Error during CREATE: Cannot create already existing table trades");
  EXPECT_EQ(errorOf("CREATE t 2 int float a b\n"),
            "Error during CREATE: Invalid column type float");
  EXPECT_EQ(errorOf("CREATE t 2 int int a a\n"), "Error during CREATE: Duplicate column name a");
  EXPECT_EQ(errorOf("CREATE t 0\n"), "Error during CREATE: A table needs at least one column");
  EXPECT_THROW((void)parse("CREATE t two int a\n"), InputError);
}

TEST_F(ParserTest, ParsesInsertRowsIntoColumns) {
  const auto cmd =
      parseAs<InsertCmd>("INSERT INTO trades 2 ROWS\n2 MSFT 420.5 false\n3 NVDA 131 true\n");
  EXPECT_EQ(cmd.table, "trades");
  ASSERT_EQ(cmd.rows.rows, 2U);
  EXPECT_EQ(cmd.rows.columns[0].values<std::int32_t>(), (std::vector<std::int32_t>{2, 3}));
  EXPECT_EQ(cmd.rows.columns[1].values<std::string>(), (std::vector<std::string>{"MSFT", "NVDA"}));
  EXPECT_EQ(cmd.rows.columns[3].values<BoolStorage>(), (std::vector<BoolStorage>{0, 1}));
}

TEST_F(ParserTest, HeaderIsReadInFullBeforeAnyCheck) {
  // As in the original, the error comes after ROWS, so the session discards the rest of the
  // line ROWS is on and the next line is read as a new command.
  EXPECT_EQ(errorOf("INSERT INTO ghost\n2 ROWS\n1 2\n"),
            "Error during INSERT: ghost does not name a table in the database");
  EXPECT_EQ(nextToken(), "1");
  EXPECT_EQ(errorOf("GENERATE FOR ghost\nhash INDEX ON id\nQUIT\n"),
            "Error during GENERATE: ghost does not name a table in the database");
  EXPECT_EQ(nextToken(), "QUIT");
  EXPECT_EQ(errorOf("JOIN ghost AND venues\nWHERE a = b AND PRINT 0\nQUIT\n"),
            "Error during JOIN: ghost does not name a table in the database");
  EXPECT_EQ(nextToken(), "QUIT");
}

TEST_F(ParserTest, VariableLengthPartsAreCheckedAsTheyAreRead) {
  EXPECT_EQ(errorOf("PRINT FROM trades 3 id\nbogus\nprice ALL\n"),
            "Error during PRINT: bogus does not name a column in trades");
  EXPECT_EQ(nextToken(), "price");
}

TEST_F(ParserTest, KeywordsAreCheckedBeforeNames) {
  EXPECT_EQ(errorOf("INSERT ONTO ghost 1 ROWS\n"),
            "Error during INSERT: Expected INTO, found ONTO");
  EXPECT_EQ(errorOf("DELETE FROM ghost WHEN a = 1\n"),
            "Error during DELETE: Expected WHERE, found WHEN");
  EXPECT_THROW((void)parse("INSERT INTO trades many ROWS\n"), InputError);
}

TEST_F(ParserTest, InsertWithMalformedValueIsAnInputError) {
  EXPECT_THROW((void)parse("INSERT INTO trades 1 ROWS\nx AAPL 1.0 true\n"), InputError);
  EXPECT_THROW((void)parse("INSERT INTO trades 1 ROWS\n1 AAPL 1.0 yes\n"), InputError);
  EXPECT_THROW((void)parse("INSERT INTO trades 2 ROWS\n1 AAPL 1.0 true\n"), InputError);
}

TEST_F(ParserTest, ParsesPrintAll) {
  const auto cmd = parseAs<PrintCmd>("PRINT FROM trades 2 price id ALL\n");
  EXPECT_EQ(cmd.table, "trades");
  EXPECT_EQ(cmd.columns, (std::vector<std::size_t>{2, 0}));
  EXPECT_FALSE(cmd.where);
}

TEST_F(ParserTest, ParsesPrintWhereWithTypedLiteral) {
  const auto cmd = parseAs<PrintCmd>("PRINT FROM trades 1 symbol WHERE price >= 100\n");
  EXPECT_EQ(cmd.where, (Predicate{2, CompareOp::GreaterEqual, Value(100.0)}));
}

TEST_F(ParserTest, PrintErrors) {
  EXPECT_EQ(errorOf("PRINT FROM ghost 1 a ALL\n"),
            "Error during PRINT: ghost does not name a table in the database");
  EXPECT_EQ(errorOf("PRINT FROM trades 2 id bogus ALL\n"),
            "Error during PRINT: bogus does not name a column in trades");
  EXPECT_EQ(nextToken(), "ALL");  // The command is abandoned at the bad column.
  EXPECT_EQ(errorOf("PRINT FROM trades 1 id WHERE bogus = 1\n"),
            "Error during PRINT: bogus does not name a column in trades");
  EXPECT_EQ(errorOf("PRINT FROM trades 1 id WHERE id ~ 1\n"),
            "Error during PRINT: Invalid comparison operator ~");
  EXPECT_EQ(errorOf("PRINT FROM trades 1 id SOME\n"),
            "Error during PRINT: Expected ALL or WHERE, found SOME");
  EXPECT_EQ(errorOf("PRINT trades 1 id ALL\n"), "Error during PRINT: Expected FROM, found trades");
  EXPECT_THROW((void)parse("PRINT FROM trades 1 id WHERE id = 1.5\n"), InputError);
}

TEST_F(ParserTest, ColumnErrorTakesPrecedenceOverOperatorError) {
  EXPECT_EQ(errorOf("DELETE FROM trades WHERE bogus ~ 1\n"),
            "Error during DELETE: bogus does not name a column in trades");
}

TEST_F(ParserTest, ParsesDelete) {
  const auto cmd = parseAs<DeleteCmd>("DELETE FROM trades WHERE buy != true\n");
  EXPECT_EQ(cmd.table, "trades");
  EXPECT_EQ(cmd.where, (Predicate{3, CompareOp::NotEqual, Value(true)}));
  EXPECT_EQ(errorOf("DELETE FROM trades id = 1\n"),
            "Error during DELETE: Expected WHERE, found id");
}

TEST_F(ParserTest, ParsesGenerate) {
  const auto cmd = parseAs<GenerateCmd>("GENERATE FOR trades bst INDEX ON price\n");
  EXPECT_EQ(cmd.kind, IndexKind::Ordered);
  EXPECT_EQ(cmd.column, 2U);
  EXPECT_EQ(parseAs<GenerateCmd>("GENERATE FOR trades hash INDEX ON id\n").kind, IndexKind::Hash);
}

TEST_F(ParserTest, GenerateErrors) {
  EXPECT_EQ(errorOf("GENERATE FOR ghost hash INDEX ON id\n"),
            "Error during GENERATE: ghost does not name a table in the database");
  EXPECT_EQ(errorOf("GENERATE FOR trades hash INDEX ON nope\n"),
            "Error during GENERATE: nope does not name a column in trades");
  EXPECT_EQ(errorOf("GENERATE FOR trades btree INDEX ON id\n"),
            "Error during GENERATE: Invalid index type btree (expected hash or bst)");
  EXPECT_EQ(errorOf("GENERATE FOR trades btree INDEX ON nope\n"),
            "Error during GENERATE: nope does not name a column in trades");
}

TEST_F(ParserTest, ParsesJoin) {
  const auto cmd = parseAs<JoinCmd>(
      "JOIN trades AND venues WHERE symbol = symbol AND PRINT 3 id 1 mic 2 symbol 1\n");
  EXPECT_EQ(cmd.left, "trades");
  EXPECT_EQ(cmd.right, "venues");
  EXPECT_EQ(cmd.leftColumn, 1U);
  EXPECT_EQ(cmd.rightColumn, 0U);
  EXPECT_EQ(cmd.output, (std::vector<JoinOutputColumn>{
                            {JoinSide::Left, 0}, {JoinSide::Right, 1}, {JoinSide::Left, 1}}));
}

TEST_F(ParserTest, JoinErrors) {
  EXPECT_EQ(errorOf("JOIN ghost AND venues WHERE a = b AND PRINT 0\n"),
            "Error during JOIN: ghost does not name a table in the database");
  EXPECT_EQ(errorOf("JOIN trades AND ghost WHERE a = b AND PRINT 0\n"),
            "Error during JOIN: ghost does not name a table in the database");
  EXPECT_EQ(errorOf("JOIN trades AND venues WHERE mic = symbol AND PRINT 0\n"),
            "Error during JOIN: mic does not name a column in trades");
  EXPECT_EQ(errorOf("JOIN trades AND venues WHERE symbol = id AND PRINT 0\n"),
            "Error during JOIN: id does not name a column in venues");
  EXPECT_EQ(errorOf("JOIN trades AND venues WHERE id = mic AND PRINT 0\n"),
            "Error during JOIN: Cannot join int column id to string column mic");
  EXPECT_EQ(errorOf("JOIN trades AND venues WHERE symbol = symbol AND PRINT 2 id 1 bogus 2\n"),
            "Error during JOIN: bogus does not name a column in venues");
  EXPECT_EQ(errorOf("JOIN trades AND venues WHERE symbol = symbol AND PRINT 1 id 3\n"),
            "Error during JOIN: Invalid table number 3 (expected 1 or 2)");
  EXPECT_EQ(errorOf("JOIN trades AND venues WHERE symbol == symbol AND PRINT 0\n"),
            "Error during JOIN: Expected =, found ==");
}

TEST_F(ParserTest, ParsesRemove) {
  EXPECT_EQ(parseAs<RemoveCmd>("REMOVE venues\n").table, "venues");
  EXPECT_EQ(errorOf("REMOVE ghost\n"),
            "Error during REMOVE: ghost does not name a table in the database");
}

TEST_F(ParserTest, ParsesAggregateWithOptionalClauses) {
  const auto plain = parseAs<AggregateCmd>("AGGREGATE FROM trades COUNT id\nQUIT\n");
  EXPECT_EQ(plain.query, (AggregateQuery{AggregateFunction::Count, 0, std::nullopt}));
  EXPECT_FALSE(plain.where);
  EXPECT_EQ(nextToken(), "QUIT");  // Clauses are never read from the next line.

  const auto grouped =
      parseAs<AggregateCmd>("AGGREGATE FROM trades AVG price GROUP BY symbol WHERE buy = true\n");
  EXPECT_EQ(grouped.query, (AggregateQuery{AggregateFunction::Avg, 2, 1}));
  EXPECT_EQ(grouped.where, (Predicate{3, CompareOp::Equal, Value(true)}));

  const auto filtered = parseAs<AggregateCmd>("AGGREGATE FROM trades MAX symbol WHERE id > 0\n");
  EXPECT_FALSE(filtered.query.groupBy);
  EXPECT_TRUE(filtered.where);
}

TEST_F(ParserTest, AggregateStopsAtATrailingComment) {
  const auto cmd = parseAs<AggregateCmd>("AGGREGATE FROM trades COUNT id # note\n");
  EXPECT_FALSE(cmd.query.groupBy);
  EXPECT_EQ(nextToken(), "#");  // Read next as a comment command, as after any command.
}

TEST_F(ParserTest, AggregateErrors) {
  EXPECT_EQ(errorOf("AGGREGATE FROM trades MEDIAN price\n"),
            "Error during AGGREGATE: Invalid aggregate function MEDIAN");
  EXPECT_EQ(errorOf("AGGREGATE FROM trades SUM symbol\n"),
            "Error during AGGREGATE: SUM requires an int or double column, but symbol is string");
  EXPECT_EQ(errorOf("AGGREGATE FROM trades AVG buy\n"),
            "Error during AGGREGATE: AVG requires an int or double column, but buy is bool");
  EXPECT_EQ(errorOf("AGGREGATE FROM trades COUNT id GROUP symbol\n"),
            "Error during AGGREGATE: Expected BY, found symbol");
  EXPECT_EQ(errorOf("AGGREGATE FROM trades COUNT id WHERE id = 1 GROUP BY symbol\n"),
            "Error during AGGREGATE: Unexpected token GROUP");
  EXPECT_EQ(errorOf("AGGREGATE FROM trades COUNT id LIMIT 5\n"),
            "Error during AGGREGATE: Unexpected token LIMIT");
}

TEST_F(ParserTest, ParsesLoad) {
  const auto cmd = parseAs<LoadCmd>("LOAD INTO trades FROM data/day1.csv\n");
  EXPECT_EQ(cmd.table, "trades");
  EXPECT_EQ(cmd.path, "data/day1.csv");
  EXPECT_EQ(errorOf("LOAD INTO ghost FROM x.csv\n"),
            "Error during LOAD: ghost does not name a table in the database");
}

TEST_F(ParserTest, ParsesExplainOfEachSupportedCommand) {
  EXPECT_TRUE(std::holds_alternative<PrintCmd>(
      parseAs<ExplainCmd>("EXPLAIN PRINT FROM trades 1 id ALL\n").target));
  EXPECT_TRUE(std::holds_alternative<DeleteCmd>(
      parseAs<ExplainCmd>("EXPLAIN DELETE FROM trades WHERE id < 3\n").target));
  EXPECT_TRUE(std::holds_alternative<JoinCmd>(
      parseAs<ExplainCmd>("EXPLAIN JOIN trades AND venues WHERE symbol = symbol AND PRINT 0\n")
          .target));
  EXPECT_TRUE(std::holds_alternative<AggregateCmd>(
      parseAs<ExplainCmd>("EXPLAIN AGGREGATE FROM trades COUNT id\n").target));
  EXPECT_EQ(errorOf("EXPLAIN REMOVE trades\n"),
            "Error during EXPLAIN: Cannot explain REMOVE (expected PRINT, DELETE, JOIN or "
            "AGGREGATE)");
  EXPECT_EQ(errorOf("EXPLAIN PRINT FROM ghost 0 ALL\n"),
            "Error during PRINT: ghost does not name a table in the database");
}

TEST_F(ParserTest, QuitAndUnknownCommands) {
  parseAs<QuitCmd>("QUIT\n");
  EXPECT_EQ(errorOf("SELECT * FROM trades\n"), "Error: unrecognized command");
  EXPECT_EQ(errorOf("quit\n"), "Error: unrecognized command");
}

TEST_F(ParserTest, EndOfInputIsAnInputError) {
  EXPECT_THROW((void)parse(""), InputError);
  EXPECT_THROW((void)parse("PRINT FROM trades 2 id"), InputError);
}

}  // namespace
}  // namespace sillyql
