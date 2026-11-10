#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sillyql/column.hpp"
#include "sillyql/index.hpp"

namespace sillyql {

struct ColumnSpec {
  std::string name;
  ColumnType type;

  friend bool operator==(const ColumnSpec&, const ColumnSpec&) = default;
};

/// Rows waiting to be appended to a table, already laid out column-wise in the table's column
/// order. INSERT and LOAD validate every value while building a batch, so bad input never
/// reaches the table.
struct RowBatch {
  std::vector<Column> columns;
  std::size_t rows = 0;
};

/// A named relation stored column-wise, with at most one secondary index.
class Table {
 public:
  /// Throws std::invalid_argument if two columns share a name.
  Table(std::string name, std::vector<ColumnSpec> schema);

  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] std::span<const ColumnSpec> schema() const noexcept { return schema_; }
  [[nodiscard]] std::size_t columnCount() const noexcept { return schema_.size(); }
  [[nodiscard]] std::size_t rowCount() const noexcept { return rowCount_; }

  [[nodiscard]] const ColumnSpec& spec(std::size_t column) const { return schema_.at(column); }
  [[nodiscard]] const Column& column(std::size_t column) const { return columns_.at(column); }

  /// Position of the column called `name`.
  [[nodiscard]] std::optional<std::size_t> findColumn(std::string_view name) const noexcept;

  /// An empty batch whose columns match this table's schema.
  [[nodiscard]] RowBatch emptyBatch() const;

  /// Appends every row of `batch` and indexes them. Returns the id of the first new row.
  /// Throws std::invalid_argument if the batch does not match the schema and std::length_error
  /// if the table would exceed the RowId range. Strong guarantee: if anything throws (including
  /// std::bad_alloc), the table and its index are unchanged.
  RowId append(RowBatch&& batch);

  /// Deletes the given rows (duplicates are ignored) with one keep-mask compaction pass over every
  /// column, then renumbers the index's postings in place. Returns the number of rows deleted.
  /// Strong guarantee: all allocation happens before the table is modified.
  std::size_t erase(std::span<const RowId> rows);

  /// Builds an index of `kind` on `column`, replacing any existing index. Returns its number of
  /// distinct keys.
  std::size_t createIndex(IndexKind kind, std::size_t column);

  /// The table's index if it is built on `column`, otherwise nullptr.
  [[nodiscard]] const Index* indexOn(std::size_t column) const noexcept;

 private:
  std::string name_;
  std::vector<ColumnSpec> schema_;
  std::vector<Column> columns_;
  std::size_t rowCount_ = 0;
  std::unique_ptr<Index> index_;
};

}  // namespace sillyql
