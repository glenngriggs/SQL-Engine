#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "sillyql/column.hpp"
#include "sillyql/compare_op.hpp"
#include "sillyql/value.hpp"

namespace sillyql {

enum class IndexKind : std::uint8_t { Hash, Ordered };

/// "hash" or "bst": the spelling used by GENERATE.
[[nodiscard]] std::string_view indexKindName(IndexKind kind) noexcept;
[[nodiscard]] std::optional<IndexKind> parseIndexKind(std::string_view name) noexcept;

/// A secondary index over one column of a table, mapping each key to its rows.
///
/// The interface is type-erased so a Table can own any index through std::unique_ptr<Index>;
/// the concrete indexes are templates over the column's storage type, so keys are stored and
/// compared as raw ints/doubles/strings rather than as variants.
///
/// Every key's postings list holds row ids in ascending order: rows are indexed in increasing
/// id order, and renumbering after a delete preserves relative order.
class Index {
 public:
  explicit Index(std::size_t column) noexcept : column_(column) {}
  virtual ~Index() = default;

  Index(const Index&) = delete;
  Index& operator=(const Index&) = delete;
  Index(Index&&) = delete;
  Index& operator=(Index&&) = delete;

  [[nodiscard]] virtual IndexKind kind() const noexcept = 0;
  [[nodiscard]] std::string_view name() const noexcept { return indexKindName(kind()); }

  /// Position of the indexed column in its table.
  [[nodiscard]] std::size_t column() const noexcept { return column_; }

  /// Whether lookup() can answer `column op key` directly.
  [[nodiscard]] virtual bool supports(CompareOp op) const noexcept = 0;

  /// Rows satisfying `column op key`, grouped by key in the index's iteration order (ascending
  /// key order for an ordered index) and ascending within a key. Throws std::logic_error if the
  /// operator is not supported.
  [[nodiscard]] virtual std::vector<RowId> lookup(CompareOp op, const Value& key) const = 0;

  /// Indexes rows [firstRow, column.size()) of `column`; rows before firstRow must already be
  /// indexed and rows from firstRow on must not be.
  virtual void extend(const Column& column, RowId firstRow) = 0;

  /// Applies a DELETE: newIds[row] is the row's id after compaction, or kNoRow if it was
  /// deleted. Keys left without rows are removed. Allocates nothing, so it cannot fail.
  virtual void renumber(std::span<const RowId> newIds) noexcept = 0;

  /// Forgets every row with id >= rowCount (rolls back a failed extend).
  virtual void truncate(RowId rowCount) noexcept = 0;

  [[nodiscard]] virtual std::size_t distinctKeys() const noexcept = 0;

 private:
  std::size_t column_;
};

/// Creates an empty index of `kind` for a column of `keyType` at position `column`.
[[nodiscard]] std::unique_ptr<Index> makeIndex(IndexKind kind, ColumnType keyType,
                                               std::size_t column);

}  // namespace sillyql
