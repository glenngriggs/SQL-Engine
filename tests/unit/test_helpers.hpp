#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "sillyql/session.hpp"
#include "sillyql/table.hpp"

namespace sillyql::test {

struct ScriptResult {
  std::string out;
  std::string diagnostics;
  int exitCode;
};

/// Runs a complete command script through a Session, capturing both output streams.
inline ScriptResult runScript(const std::string& script, SessionOptions options = {}) {
  std::istringstream in(script);
  std::ostringstream out;
  std::ostringstream diagnostics;
  Session session(in, out, diagnostics, options);
  const int exitCode = session.run();
  return {out.str(), diagnostics.str(), exitCode};
}

/// A batch for `table` built from rows of literal tokens.
inline RowBatch batchOf(const Table& table, const std::vector<std::vector<std::string>>& rows) {
  RowBatch batch = table.emptyBatch();
  for (const std::vector<std::string>& row : rows) {
    if (row.size() != table.columnCount()) {
      throw std::invalid_argument("row has the wrong number of values");
    }
    for (std::size_t column = 0; column < row.size(); ++column) {
      if (!batch.columns[column].appendText(std::string(row[column]))) {
        throw std::invalid_argument("invalid literal " + row[column]);
      }
    }
    ++batch.rows;
  }
  return batch;
}

/// The value of every selected row in `column`, as text, in the given order.
inline std::vector<std::string> columnText(const Table& table, std::size_t column,
                                           const std::vector<RowId>& rows) {
  std::vector<std::string> text;
  for (const RowId row : rows) {
    std::ostringstream cell;
    table.column(column).write(cell, row);
    text.push_back(cell.str());
  }
  return text;
}

}  // namespace sillyql::test
