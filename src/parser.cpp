#include "sillyql/parser.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "sillyql/errors.hpp"
#include "sillyql/literal.hpp"

namespace sillyql {
namespace {

/// Upper bound on capacity reserved from a row count in the input, so a typo such as
/// "INSERT INTO t 99999999999 ROWS" cannot trigger a huge allocation before any row is read.
constexpr std::size_t kMaxReservedRows = std::size_t{1} << 16;

void expectKeyword(std::string_view verb, std::string_view token, std::string_view keyword) {
  if (token != keyword) {
    throw SyntaxError(verb, "Expected " + std::string(keyword) + ", found " + std::string(token));
  }
}

std::size_t toColumn(std::string_view verb, const Table& table, std::string_view name) {
  if (const auto column = table.findColumn(name)) {
    return *column;
  }
  throw ColumnNotFoundError(verb, name, table.name());
}

/// A count the grammar requires; anything else is an unreadable number, as in the original.
std::size_t toCount(std::string_view token) {
  const auto count = parseCount(token);
  if (!count) {
    throw InputError();
  }
  return *count;
}

}  // namespace

Command Parser::parse(std::string_view keyword) {
  if (keyword.starts_with('#')) {
    tokens_.skipLine();
    return CommentCmd{};
  }
  if (keyword == CreateCmd::kName) {
    return parseCreate();
  }
  if (keyword == InsertCmd::kName) {
    return parseInsert();
  }
  if (keyword == PrintCmd::kName) {
    return parsePrint();
  }
  if (keyword == DeleteCmd::kName) {
    return parseDelete();
  }
  if (keyword == GenerateCmd::kName) {
    return parseGenerate();
  }
  if (keyword == JoinCmd::kName) {
    return parseJoin();
  }
  if (keyword == RemoveCmd::kName) {
    return parseRemove();
  }
  if (keyword == AggregateCmd::kName) {
    return parseAggregate();
  }
  if (keyword == LoadCmd::kName) {
    return parseLoad();
  }
  if (keyword == ExplainCmd::kName) {
    return parseExplain();
  }
  if (keyword == QuitCmd::kName) {
    return QuitCmd{};
  }
  throw UnknownCommandError();
}

// CREATE <table> <N> <type>... <name>...
CreateCmd Parser::parseCreate() {
  constexpr std::string_view kVerb = CreateCmd::kName;
  auto [table, countText] = readHeader<2>();
  const std::size_t count = toCount(countText);
  if (catalog_.contains(table)) {
    throw TableExistsError(table);
  }
  if (count == 0) {
    throw SemanticError(kVerb, "A table needs at least one column");
  }

  std::vector<ColumnType> types;
  for (std::size_t i = 0; i < count; ++i) {
    const std::string typeName = tokens_.next();
    const auto type = parseColumnType(typeName);
    if (!type) {
      throw SemanticError(kVerb, "Invalid column type " + typeName);
    }
    types.push_back(*type);
  }
  CreateCmd cmd{std::move(table), {}};
  for (const ColumnType type : types) {
    std::string name = tokens_.next();
    const bool duplicate =
        std::any_of(cmd.columns.begin(), cmd.columns.end(),
                    [&name](const ColumnSpec& spec) { return spec.name == name; });
    if (duplicate) {
      throw SemanticError(kVerb, "Duplicate column name " + name);
    }
    cmd.columns.push_back({std::move(name), type});
  }
  return cmd;
}

// INSERT INTO <table> <N> ROWS, then N rows
InsertCmd Parser::parseInsert() {
  constexpr std::string_view kVerb = InsertCmd::kName;
  const auto [into, tableName, rowsText, rowsKeyword] = readHeader<4>();
  expectKeyword(kVerb, into, "INTO");
  expectKeyword(kVerb, rowsKeyword, "ROWS");
  const Table& table = catalog_.require(kVerb, tableName);
  const std::size_t rows = toCount(rowsText);

  InsertCmd cmd{table.name(), table.emptyBatch()};
  for (Column& column : cmd.rows.columns) {
    column.reserve(std::min(rows, kMaxReservedRows));
  }
  for (std::size_t row = 0; row < rows; ++row) {
    for (Column& column : cmd.rows.columns) {
      if (!column.appendText(tokens_.next())) {
        throw InputError();
      }
    }
  }
  cmd.rows.rows = rows;
  return cmd;
}

// PRINT FROM <table> <N> <col>... {ALL | WHERE <col> <op> <value>}
PrintCmd Parser::parsePrint() {
  constexpr std::string_view kVerb = PrintCmd::kName;
  const auto [from, tableName, countText] = readHeader<3>();
  expectKeyword(kVerb, from, "FROM");
  const Table& table = catalog_.require(kVerb, tableName);
  const std::size_t count = toCount(countText);

  PrintCmd cmd{table.name(), {}, std::nullopt};
  for (std::size_t i = 0; i < count; ++i) {
    cmd.columns.push_back(toColumn(kVerb, table, tokens_.next()));
  }
  const std::string mode = tokens_.next();
  if (mode == "WHERE") {
    cmd.where = readPredicate(kVerb, table);
  } else if (mode != "ALL") {
    throw SyntaxError(kVerb, "Expected ALL or WHERE, found " + mode);
  }
  return cmd;
}

// DELETE FROM <table> WHERE <col> <op> <value>
DeleteCmd Parser::parseDelete() {
  constexpr std::string_view kVerb = DeleteCmd::kName;
  const auto [from, tableName, where, column, op] = readHeader<5>();
  expectKeyword(kVerb, from, "FROM");
  expectKeyword(kVerb, where, "WHERE");
  const Table& table = catalog_.require(kVerb, tableName);
  return DeleteCmd{table.name(), bindPredicate(kVerb, table, column, op)};
}

// GENERATE FOR <table> {hash | bst} INDEX ON <col>
GenerateCmd Parser::parseGenerate() {
  constexpr std::string_view kVerb = GenerateCmd::kName;
  const auto [forKeyword, tableName, kindName, indexKeyword, on, columnName] = readHeader<6>();
  expectKeyword(kVerb, forKeyword, "FOR");
  expectKeyword(kVerb, indexKeyword, "INDEX");
  expectKeyword(kVerb, on, "ON");
  const Table& table = catalog_.require(kVerb, tableName);
  const std::size_t column = toColumn(kVerb, table, columnName);
  const auto kind = parseIndexKind(kindName);
  if (!kind) {
    throw SemanticError(kVerb, "Invalid index type " + kindName + " (expected hash or bst)");
  }
  return GenerateCmd{table.name(), *kind, column};
}

// JOIN <t1> AND <t2> WHERE <c1> = <c2> AND PRINT <N> {<col> {1|2}}...
JoinCmd Parser::parseJoin() {
  constexpr std::string_view kVerb = JoinCmd::kName;
  const auto [leftName, and1, rightName, where, leftColumnName, equals, rightColumnName, and2,
              print, countText] = readHeader<10>();
  expectKeyword(kVerb, and1, "AND");
  expectKeyword(kVerb, where, "WHERE");
  expectKeyword(kVerb, equals, "=");
  expectKeyword(kVerb, and2, "AND");
  expectKeyword(kVerb, print, "PRINT");
  const Table& left = catalog_.require(kVerb, leftName);
  const Table& right = catalog_.require(kVerb, rightName);
  const std::size_t leftColumn = toColumn(kVerb, left, leftColumnName);
  const std::size_t rightColumn = toColumn(kVerb, right, rightColumnName);
  const std::size_t count = toCount(countText);

  const ColumnSpec& leftSpec = left.spec(leftColumn);
  const ColumnSpec& rightSpec = right.spec(rightColumn);
  if (leftSpec.type != rightSpec.type) {
    throw SemanticError(kVerb, "Cannot join " + std::string(columnTypeName(leftSpec.type)) +
                                   " column " + leftSpec.name + " to " +
                                   std::string(columnTypeName(rightSpec.type)) + " column " +
                                   rightSpec.name);
  }

  JoinCmd cmd{left.name(), right.name(), leftColumn, rightColumn, {}};
  for (std::size_t i = 0; i < count; ++i) {
    const std::string name = tokens_.next();
    const std::size_t tableNumber = toCount(tokens_.next());
    if (tableNumber != 1 && tableNumber != 2) {
      throw SemanticError(
          kVerb, "Invalid table number " + std::to_string(tableNumber) + " (expected 1 or 2)");
    }
    const JoinSide side = tableNumber == 1 ? JoinSide::Left : JoinSide::Right;
    cmd.output.push_back({side, toColumn(kVerb, side == JoinSide::Left ? left : right, name)});
  }
  return cmd;
}

// REMOVE <table>
RemoveCmd Parser::parseRemove() {
  return RemoveCmd{catalog_.require(RemoveCmd::kName, tokens_.next()).name()};
}

// AGGREGATE FROM <table> <FUNC> <col> [GROUP BY <col>] [WHERE <col> <op> <value>]
AggregateCmd Parser::parseAggregate() {
  constexpr std::string_view kVerb = AggregateCmd::kName;
  const auto [from, tableName, functionName, columnName] = readHeader<4>();
  expectKeyword(kVerb, from, "FROM");
  const Table& table = catalog_.require(kVerb, tableName);
  const std::size_t column = toColumn(kVerb, table, columnName);
  const auto function = parseAggregateFunction(functionName);
  if (!function) {
    throw SemanticError(kVerb, "Invalid aggregate function " + functionName);
  }
  const ColumnSpec& spec = table.spec(column);
  if (!acceptsColumnType(*function, spec.type)) {
    throw SemanticError(kVerb, functionName + " requires an int or double column, but " +
                                   spec.name + " is " + std::string(columnTypeName(spec.type)));
  }

  AggregateCmd cmd{table.name(), {*function, column, std::nullopt}, std::nullopt};
  // The optional clauses must be on the command's own line.
  auto nextOnLine = [this]() -> std::optional<std::string> {
    return tokens_.hasMoreOnLine() ? std::optional<std::string>(tokens_.next()) : std::nullopt;
  };
  std::optional<std::string> clause = nextOnLine();
  if (clause == "GROUP") {
    expectKeyword(kVerb, tokens_.next(), "BY");
    cmd.query.groupBy = toColumn(kVerb, table, tokens_.next());
    clause = nextOnLine();
  }
  if (clause == "WHERE") {
    cmd.where = readPredicate(kVerb, table);
    clause = nextOnLine();
  }
  if (clause) {
    throw SyntaxError(kVerb, "Unexpected token " + *clause);
  }
  return cmd;
}

// LOAD INTO <table> FROM <path>
LoadCmd Parser::parseLoad() {
  constexpr std::string_view kVerb = LoadCmd::kName;
  auto [into, tableName, from, path] = readHeader<4>();
  expectKeyword(kVerb, into, "INTO");
  expectKeyword(kVerb, from, "FROM");
  return LoadCmd{catalog_.require(kVerb, tableName).name(), std::move(path)};
}

// EXPLAIN <PRINT | DELETE | JOIN | AGGREGATE command>
ExplainCmd Parser::parseExplain() {
  const std::string keyword = tokens_.next();
  if (keyword == PrintCmd::kName) {
    return ExplainCmd{parsePrint()};
  }
  if (keyword == DeleteCmd::kName) {
    return ExplainCmd{parseDelete()};
  }
  if (keyword == JoinCmd::kName) {
    return ExplainCmd{parseJoin()};
  }
  if (keyword == AggregateCmd::kName) {
    return ExplainCmd{parseAggregate()};
  }
  throw SyntaxError(ExplainCmd::kName,
                    "Cannot explain " + keyword + " (expected PRINT, DELETE, JOIN or AGGREGATE)");
}

template <std::size_t N>
std::array<std::string, N> Parser::readHeader() {
  std::array<std::string, N> header;
  for (std::string& token : header) {
    token = tokens_.next();
  }
  return header;
}

Predicate Parser::readPredicate(std::string_view verb, const Table& table) {
  const auto [column, op] = readHeader<2>();
  return bindPredicate(verb, table, column, op);
}

Predicate Parser::bindPredicate(std::string_view verb, const Table& table, std::string_view column,
                                std::string_view op) {
  const std::size_t position = toColumn(verb, table, column);
  const auto compareOp = parseCompareOp(op);
  if (!compareOp) {
    throw SyntaxError(verb, "Invalid comparison operator " + std::string(op));
  }
  auto value = parseValue(table.spec(position).type, tokens_.next());
  if (!value) {
    throw InputError();
  }
  return Predicate{position, *compareOp, std::move(*value)};
}

}  // namespace sillyql
