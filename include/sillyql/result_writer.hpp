#pragma once

#include <iosfwd>
#include <ostream>
#include <string_view>

#include "sillyql/aggregate.hpp"
#include "sillyql/column.hpp"
#include "sillyql/value.hpp"

namespace sillyql {

/// Formats everything the engine prints to standard output.
///
/// Result rows (and their header line) follow the legacy convention of a space after every value
/// and are suppressed in quiet mode: cell() and endRow() write nothing then, so a command cannot
/// leak rows into quiet output. Prompts, summaries and error messages are always printed.
class ResultWriter {
 public:
  ResultWriter(std::ostream& out, bool quiet) noexcept : out_(out), quiet_(quiet) {}

  /// Whether result rows are written at all. Purely an optimisation hint: callers may check it
  /// once per command to skip producing rows that would be discarded.
  [[nodiscard]] bool showRows() const noexcept { return !quiet_; }

  /// The "% " prompt printed before each command is read.
  void prompt() { out_ << "% "; }

  /// Writes the arguments followed by a newline.
  template <class... Parts>
  void line(const Parts&... parts) {
    (out_ << ... << parts) << '\n';
  }

  /// One cell of a result row (or header), followed by the separating space.
  void cell(std::string_view text);
  void cell(const Column& column, RowId row);
  void cell(const Value& value);
  void cell(const AggregateValue& value);

  /// Ends a result row.
  void endRow();

  void flush() { out_.flush(); }

 private:
  std::ostream& out_;
  bool quiet_;
};

}  // namespace sillyql
