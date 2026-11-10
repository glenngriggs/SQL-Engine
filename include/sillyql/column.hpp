#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "sillyql/value.hpp"

namespace sillyql {

/// Position of a row in a table. 32 bits halve the footprint of index postings and selection
/// vectors compared to size_t; a table holds at most 2^32 - 1 rows.
using RowId = std::uint32_t;

/// Never a valid row id (a table's largest row id is 2^32 - 2). Marks a deleted row in the
/// renumbering table that DELETE hands to the index.
inline constexpr RowId kNoRow = std::numeric_limits<RowId>::max();

/// Physical representation of a bool cell: one byte. std::vector<bool> is a bit-packed proxy
/// container whose elements cannot be referenced or scanned like plain values.
using BoolStorage = std::uint8_t;

/// The element types a Column can store, one per ColumnType.
template <class S>
concept StorageType = std::same_as<S, std::int32_t> || std::same_as<S, double> ||
                      std::same_as<S, BoolStorage> || std::same_as<S, std::string>;

/// Calls `fn(std::type_identity<S>{})`, where S is the storage type of `type`. This is the one
/// place a runtime ColumnType selects a template instantiation (storage, indexes, literals).
template <class Fn>
auto visitStorageType(ColumnType type, Fn&& fn) {
  switch (type) {
    case ColumnType::Int:
      return std::forward<Fn>(fn)(std::type_identity<std::int32_t>{});
    case ColumnType::Double:
      return std::forward<Fn>(fn)(std::type_identity<double>{});
    case ColumnType::Bool:
      return std::forward<Fn>(fn)(std::type_identity<BoolStorage>{});
    case ColumnType::String:
      return std::forward<Fn>(fn)(std::type_identity<std::string>{});
  }
  throw std::invalid_argument("invalid column type");
}

/// The logical column type stored as `S`.
template <StorageType S>
constexpr ColumnType columnTypeOf() noexcept {
  if constexpr (std::is_same_v<S, std::int32_t>) {
    return ColumnType::Int;
  } else if constexpr (std::is_same_v<S, double>) {
    return ColumnType::Double;
  } else if constexpr (std::is_same_v<S, BoolStorage>) {
    return ColumnType::Bool;
  } else {
    return ColumnType::String;
  }
}

/// `value` in storage representation `S`: a reference into the Value for int, double and string
/// (no copy), a byte for bool. Throws TypeMismatchError if `value` is not of S's logical type.
template <StorageType S>
[[nodiscard]] decltype(auto) storageKey(const Value& value) {
  if constexpr (std::is_same_v<S, BoolStorage>) {
    return static_cast<BoolStorage>(value.get<bool>());
  } else {
    return value.get<S>();
  }
}

/// The Value of a stored element.
template <StorageType S>
[[nodiscard]] Value toValue(const S& element) {
  if constexpr (std::is_same_v<S, BoolStorage>) {
    return Value(element != 0);
  } else {
    return Value(element);
  }
}

/// Parses `text` as an element of type `S` (see literal.hpp for the accepted spellings).
template <StorageType S>
[[nodiscard]] std::optional<S> parseElement(std::string_view text);

/// Parses `text` as a literal of `type`; nullopt if it is not a valid literal of that type.
[[nodiscard]] std::optional<Value> parseValue(ColumnType type, std::string_view text);

/// Writes a stored element with the engine's output formatting (bools as true/false).
template <StorageType S>
void writeElement(std::ostream& os, const S& element) {
  if constexpr (std::is_same_v<S, BoolStorage>) {
    os << (element != 0 ? "true" : "false");
  } else {
    os << element;
  }
}

/// One attribute of a table, stored contiguously as a typed vector (column-store layout).
///
/// Scans touch only the bytes of the column they filter on, and the typed vector lets the scan
/// kernels compare raw ints/doubles instead of dispatching on a variant per cell.
class Column {
 public:
  using Storage = std::variant<std::vector<std::int32_t>, std::vector<double>,
                               std::vector<BoolStorage>, std::vector<std::string>>;

  explicit Column(ColumnType type);

  [[nodiscard]] ColumnType type() const noexcept { return static_cast<ColumnType>(data_.index()); }
  [[nodiscard]] std::size_t size() const;

  void reserve(std::size_t capacity);

  /// Appends `value`; throws TypeMismatchError if its type differs from the column's.
  void append(const Value& value);

  /// Parses `text` as a literal of the column's type and appends it; a string is moved in, not
  /// copied. Returns false, leaving the column and `text` unchanged, if `text` is not a valid
  /// literal.
  [[nodiscard]] bool appendText(std::string&& text);

  /// Allocates everything appendFrom(other) needs, so that the appendFrom call cannot throw.
  /// Capacity grows geometrically, so repeated small appends stay amortised O(1) per element.
  void reserveFor(const Column& other);

  /// Moves every element of `other` (same type) to the end of this column and empties `other`.
  /// A column that has never allocated (as in a new table) takes over `other`'s buffer in O(1).
  void appendFrom(Column&& other);

  /// Keeps only the first `size` elements. Destroys elements only, so it does not fail.
  void truncate(std::size_t size);

  [[nodiscard]] Value valueAt(RowId row) const;
  void write(std::ostream& os, RowId row) const;

  /// Keeps row i iff keep[i] != 0, preserving order. `keep` must have size() entries. Moves and
  /// destroys elements only (no allocation), so it does not fail.
  void compact(std::span<const std::uint8_t> keep);

  /// The typed storage; `S` must match the column type (std::bad_variant_access otherwise).
  template <StorageType S>
  [[nodiscard]] const std::vector<S>& values() const {
    return std::get<std::vector<S>>(data_);
  }

  /// Invokes `visitor` with the typed storage vector (compile-time dispatch on the element type).
  template <class Visitor>
  decltype(auto) visit(Visitor&& visitor) const {
    return std::visit(std::forward<Visitor>(visitor), data_);
  }

 private:
  Storage data_;
};

/// The element type of a storage vector passed to a Column::visit visitor.
template <class Vector>
using ElementOf = typename std::remove_cvref_t<Vector>::value_type;

}  // namespace sillyql
