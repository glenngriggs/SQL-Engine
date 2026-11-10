#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sillyql/table.hpp"

/// Bulk loading of CSV files into an existing table (the LOAD command).
namespace sillyql {

/// Splits one CSV record into fields. A field may be wrapped in double quotes, inside which
/// commas are literal and "" is an escaped quote; whitespace around fields is trimmed. Returns
/// nullopt for an unterminated quoted field or text between a closing quote and the next comma.
[[nodiscard]] std::optional<std::vector<std::string>> splitCsvRecord(std::string_view line);

/// Reads CSV text for `table`. The first non-blank line is a header naming every column of the
/// table exactly once, in any order; each further non-blank line is a row. Every value is
/// type-checked before anything is returned, so a failed load leaves the table untouched. A
/// string value must be a single token like everywhere else in the language: non-empty and
/// without whitespace. Throws LoadError describing the first problem found.
[[nodiscard]] RowBatch readCsv(std::istream& in, const Table& table);

/// readCsv on the regular file at `path`.
[[nodiscard]] RowBatch readCsvFile(const std::string& path, const Table& table);

}  // namespace sillyql
