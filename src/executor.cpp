#include "sillyql/executor.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "sillyql/csv_loader.hpp"
#include "sillyql/errors.hpp"
#include "sillyql/join.hpp"
#include "sillyql/planner.hpp"

namespace sillyql {
namespace {

/// The WHERE clause of a command, for planScan (which borrows it); nullptr when there is none.
const Predicate* filterOf(const std::optional<Predicate>& where) noexcept {
  return where ? &*where : nullptr;
}

const Predicate* filterOf(const Predicate& where) noexcept {
  return &where;
}

}  // namespace

Outcome Executor::execute(Command command) {
  const bool quit = std::holds_alternative<QuitCmd>(command);
  std::visit([this](auto&& cmd) { run(std::forward<decltype(cmd)>(cmd)); }, std::move(command));
  return quit ? Outcome::Quit : Outcome::Continue;
}

void Executor::run(const CommentCmd& /*cmd*/) {}

void Executor::run(CreateCmd&& cmd) {
  const Table& table = catalog_.create(std::move(cmd.table), std::move(cmd.columns));
  std::string text = "New table " + table.name() + " with column(s)";
  for (const ColumnSpec& column : table.schema()) {
    text += ' ';
    text += column.name;
  }
  writer_.line(text, " created");
}

void Executor::run(InsertCmd&& cmd) {
  Table& table = catalog_.require(InsertCmd::kName, cmd.table);
  const std::size_t rows = cmd.rows.rows;
  const RowId first = table.append(std::move(cmd.rows));
  reportAppend("Added", "to", rows, first, table);
}

void Executor::run(const PrintCmd& cmd) {
  const Table& table = catalog_.require(PrintCmd::kName, cmd.table);
  const std::vector<RowId> rows = executeScan(table, planScan(table, filterOf(cmd.where)));
  if (writer_.showRows()) {
    for (const std::size_t column : cmd.columns) {
      writer_.cell(table.spec(column).name);
    }
    writer_.endRow();
    for (const RowId row : rows) {
      for (const std::size_t column : cmd.columns) {
        writer_.cell(table.column(column), row);
      }
      writer_.endRow();
    }
  }
  writer_.line("Printed ", rows.size(), " matching rows from ", table.name());
}

void Executor::run(const DeleteCmd& cmd) {
  Table& table = catalog_.require(DeleteCmd::kName, cmd.table);
  const std::vector<RowId> rows = executeScan(table, planScan(table, filterOf(cmd.where)));
  const std::size_t deleted = table.erase(rows);
  writer_.line("Deleted ", deleted, " rows from ", table.name());
}

void Executor::run(const GenerateCmd& cmd) {
  Table& table = catalog_.require(GenerateCmd::kName, cmd.table);
  const std::size_t distinctKeys = table.createIndex(cmd.kind, cmd.column);
  writer_.line("Generated ", indexKindName(cmd.kind), " index for table ", table.name(),
               " on column ", table.spec(cmd.column).name, ", with ", distinctKeys,
               " distinct keys");
}

void Executor::run(const JoinCmd& cmd) {
  const Table& left = catalog_.require(JoinCmd::kName, cmd.left);
  const Table& right = catalog_.require(JoinCmd::kName, cmd.right);
  const auto source = [&](JoinSide side) -> const Table& {
    return side == JoinSide::Left ? left : right;
  };

  const bool showRows = writer_.showRows();
  if (showRows) {
    for (const JoinOutputColumn& output : cmd.output) {
      writer_.cell(source(output.side).spec(output.column).name);
    }
    writer_.endRow();
  }
  const std::size_t matches =
      executeJoin(left, cmd.leftColumn, right, cmd.rightColumn, planJoin(right, cmd.rightColumn),
                  [&](RowId leftRow, RowId rightRow) {
                    if (!showRows) {
                      return;
                    }
                    for (const JoinOutputColumn& output : cmd.output) {
                      const RowId row = output.side == JoinSide::Left ? leftRow : rightRow;
                      writer_.cell(source(output.side).column(output.column), row);
                    }
                    writer_.endRow();
                  });
  writer_.line("Printed ", matches, " rows from joining ", left.name(), " to ", right.name());
}

void Executor::run(const RemoveCmd& cmd) {
  if (!catalog_.drop(cmd.table)) {
    throw TableNotFoundError(RemoveCmd::kName, cmd.table);
  }
  writer_.line("Table ", cmd.table, " removed");
}

void Executor::run(const AggregateCmd& cmd) {
  const Table& table = catalog_.require(AggregateCmd::kName, cmd.table);
  const AggregateResult result =
      aggregate(table, cmd.query, executeScan(table, planScan(table, filterOf(cmd.where))));
  if (writer_.showRows()) {
    if (cmd.query.groupBy) {
      writer_.cell(table.spec(*cmd.query.groupBy).name);
    }
    writer_.cell(std::string(aggregateFunctionName(cmd.query.function)) + '(' +
                 table.spec(cmd.query.column).name + ')');
    writer_.endRow();
    for (const AggregateGroup& group : result.groups) {
      if (group.key) {
        writer_.cell(*group.key);
      }
      writer_.cell(group.value);
      writer_.endRow();
    }
  }
  writer_.line("Aggregated ", result.inputRows, " rows from ", table.name());
}

void Executor::run(const LoadCmd& cmd) {
  Table& table = catalog_.require(LoadCmd::kName, cmd.table);
  RowBatch batch = readCsvFile(cmd.path, table);
  const std::size_t rows = batch.rows;
  const RowId first = table.append(std::move(batch));
  reportAppend("Loaded", "into", rows, first, table);
}

void Executor::run(const ExplainCmd& cmd) {
  const std::string plan = std::visit(
      [this](const auto& target) {
        using Target = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<Target, JoinCmd>) {
          const Table& right = catalog_.require(Target::kName, target.right);
          return describeJoin(right, target.rightColumn, planJoin(right, target.rightColumn));
        } else {
          const Table& table = catalog_.require(Target::kName, target.table);
          return describe(table, planScan(table, filterOf(target.where)));
        }
      },
      cmd.target);
  writer_.line(plan);
}

void Executor::run(const QuitCmd& /*cmd*/) {
  writer_.line("Thanks for being silly!");
}

void Executor::reportAppend(std::string_view verb, std::string_view preposition, std::size_t rows,
                            RowId first, const Table& table) {
  if (rows == 0) {
    writer_.line(verb, " 0 rows ", preposition, ' ', table.name());
    return;
  }
  writer_.line(verb, ' ', rows, " rows ", preposition, ' ', table.name(), " from position ", first,
               " to ", first + rows - 1);
}

}  // namespace sillyql
