#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "sillyql/aggregate.hpp"
#include "sillyql/index.hpp"
#include "sillyql/predicate.hpp"
#include "sillyql/table.hpp"

/// The command AST produced by the Parser. Names of tables are kept as strings; columns are
/// already bound to positions and literals already typed by their column.
namespace sillyql {

/// A '#' line. Produces no output, but the prompt is still printed for it.
struct CommentCmd {};

/// CREATE <table> <N> <type1> ... <typeN> <col1> ... <colN>
struct CreateCmd {
  static constexpr std::string_view kName = "CREATE";
  std::string table;
  std::vector<ColumnSpec> columns;
};

/// INSERT INTO <table> <N> ROWS, followed by N rows of values.
struct InsertCmd {
  static constexpr std::string_view kName = "INSERT";
  std::string table;
  RowBatch rows;
};

/// PRINT FROM <table> <N> <col1> ... <colN> {ALL | WHERE <col> <op> <value>}
struct PrintCmd {
  static constexpr std::string_view kName = "PRINT";
  std::string table;
  std::vector<std::size_t> columns;
  std::optional<Predicate> where;
};

/// DELETE FROM <table> WHERE <col> <op> <value>
struct DeleteCmd {
  static constexpr std::string_view kName = "DELETE";
  std::string table;
  Predicate where;
};

/// GENERATE FOR <table> {hash | bst} INDEX ON <col>
struct GenerateCmd {
  static constexpr std::string_view kName = "GENERATE";
  std::string table;
  IndexKind kind;
  std::size_t column;
};

enum class JoinSide : std::uint8_t { Left, Right };

struct JoinOutputColumn {
  JoinSide side;
  std::size_t column;

  friend bool operator==(const JoinOutputColumn&, const JoinOutputColumn&) = default;
};

/// JOIN <t1> AND <t2> WHERE <c1> = <c2> AND PRINT <N> {<col> {1|2}}...
struct JoinCmd {
  static constexpr std::string_view kName = "JOIN";
  std::string left;
  std::string right;
  std::size_t leftColumn;
  std::size_t rightColumn;
  std::vector<JoinOutputColumn> output;
};

/// REMOVE <table>
struct RemoveCmd {
  static constexpr std::string_view kName = "REMOVE";
  std::string table;
};

/// AGGREGATE FROM <table> <FUNC> <col> [GROUP BY <col>] [WHERE <col> <op> <value>]
struct AggregateCmd {
  static constexpr std::string_view kName = "AGGREGATE";
  std::string table;
  AggregateQuery query;
  std::optional<Predicate> where;
};

/// LOAD INTO <table> FROM <path>
struct LoadCmd {
  static constexpr std::string_view kName = "LOAD";
  std::string table;
  std::string path;
};

/// EXPLAIN <PRINT | DELETE | JOIN | AGGREGATE command>
struct ExplainCmd {
  static constexpr std::string_view kName = "EXPLAIN";
  std::variant<PrintCmd, DeleteCmd, JoinCmd, AggregateCmd> target;
};

/// QUIT
struct QuitCmd {
  static constexpr std::string_view kName = "QUIT";
};

using Command = std::variant<CommentCmd, CreateCmd, InsertCmd, PrintCmd, DeleteCmd, GenerateCmd,
                             JoinCmd, RemoveCmd, AggregateCmd, LoadCmd, ExplainCmd, QuitCmd>;

}  // namespace sillyql
