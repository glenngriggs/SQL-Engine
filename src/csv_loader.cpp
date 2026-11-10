#include "sillyql/csv_loader.hpp"

#include <filesystem>
#include <fstream>
#include <istream>
#include <system_error>
#include <utility>

#include "sillyql/errors.hpp"

namespace sillyql {
namespace {

bool isBlank(char c) noexcept {
  return c == ' ' || c == '\t';
}

std::string_view trim(std::string_view text) noexcept {
  while (!text.empty() && isBlank(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && isBlank(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

/// Reads the next line that is not blank, without its line terminator. Counts every physical
/// line in `lineNumber` so errors can point at the right line.
bool nextRecordLine(std::istream& in, std::string& line, std::size_t& lineNumber) {
  while (std::getline(in, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!trim(line).empty()) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> splitOrThrow(std::string_view line, std::size_t lineNumber) {
  auto fields = splitCsvRecord(line);
  if (!fields) {
    throw LoadError("Line " + std::to_string(lineNumber) + ": malformed quoted field");
  }
  return std::move(*fields);
}

/// Maps each header position to a table column, checking that the header names every column of
/// the table exactly once.
std::vector<std::size_t> bindHeader(const std::vector<std::string>& header, const Table& table) {
  std::vector<std::size_t> target;
  std::vector<bool> seen(table.columnCount(), false);
  target.reserve(header.size());
  for (const std::string& name : header) {
    const auto column = table.findColumn(name);
    if (!column) {
      throw LoadError("Header column " + name + " does not name a column in " + table.name());
    }
    if (seen[*column]) {
      throw LoadError("Header lists column " + name + " more than once");
    }
    seen[*column] = true;
    target.push_back(*column);
  }
  for (std::size_t column = 0; column < seen.size(); ++column) {
    if (!seen[column]) {
      throw LoadError("Header does not list column " + table.spec(column).name + " of " +
                      table.name());
    }
  }
  return target;
}

}  // namespace

std::optional<std::vector<std::string>> splitCsvRecord(std::string_view line) {
  std::vector<std::string> fields;
  std::size_t pos = 0;
  while (true) {
    while (pos < line.size() && isBlank(line[pos])) {
      ++pos;
    }
    std::string field;
    if (pos < line.size() && line[pos] == '"') {
      ++pos;
      while (true) {
        if (pos >= line.size()) {
          return std::nullopt;
        }
        if (line[pos] == '"') {
          if (pos + 1 < line.size() && line[pos + 1] == '"') {
            field += '"';
            pos += 2;
            continue;
          }
          ++pos;
          break;
        }
        field += line[pos++];
      }
      while (pos < line.size() && isBlank(line[pos])) {
        ++pos;
      }
      if (pos < line.size() && line[pos] != ',') {
        return std::nullopt;
      }
    } else {
      const std::size_t start = pos;
      while (pos < line.size() && line[pos] != ',') {
        ++pos;
      }
      field = trim(line.substr(start, pos - start));
    }
    fields.push_back(std::move(field));
    if (pos >= line.size()) {
      return fields;
    }
    ++pos;  // Skip the comma.
  }
}

RowBatch readCsv(std::istream& in, const Table& table) {
  std::string line;
  std::size_t lineNumber = 0;
  if (!nextRecordLine(in, line, lineNumber)) {
    throw LoadError("File has no header row");
  }
  const std::vector<std::string> header = splitOrThrow(line, lineNumber);
  const std::vector<std::size_t> target = bindHeader(header, table);

  RowBatch batch = table.emptyBatch();
  while (nextRecordLine(in, line, lineNumber)) {
    std::vector<std::string> fields = splitOrThrow(line, lineNumber);
    if (fields.size() != header.size()) {
      throw LoadError("Line " + std::to_string(lineNumber) + " has " +
                      std::to_string(fields.size()) + " fields, expected " +
                      std::to_string(header.size()));
    }
    for (std::size_t i = 0; i < fields.size(); ++i) {
      Column& column = batch.columns[target[i]];
      if (!column.appendText(std::move(fields[i]))) {
        throw LoadError("Line " + std::to_string(lineNumber) + ": '" + fields[i] +
                        "' is not a valid " + std::string(columnTypeName(column.type())) +
                        " value for column " + table.spec(target[i]).name);
      }
    }
    ++batch.rows;
  }
  return batch;
}

RowBatch readCsvFile(const std::string& path, const Table& table) {
  std::error_code error;
  std::ifstream file;
  if (std::filesystem::is_regular_file(path, error)) {
    file.open(path);
  }
  if (!file.is_open()) {
    throw LoadError("Cannot open file " + path);
  }
  return readCsv(file, table);
}

}  // namespace sillyql
