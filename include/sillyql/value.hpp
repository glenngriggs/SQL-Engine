#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "sillyql/errors.hpp"

namespace sillyql {

/// Logical type of a column. The enumerator order matches the alternative order of Value's
/// variant (and of Column's storage variant), so a variant index converts directly to a type.
enum class ColumnType : std::uint8_t { Int, Double, Bool, String };

/// "int", "double", "bool" or "string": the spelling used by CREATE.
[[nodiscard]] std::string_view columnTypeName(ColumnType type) noexcept;
[[nodiscard]] std::optional<ColumnType> parseColumnType(std::string_view name) noexcept;

/// A single typed cell value: a 32-bit int, a double, a bool or a string.
///
/// Value is the type-erased currency between the parser, the planner and EXPLAIN (a WHERE literal,
/// an aggregate's MIN/MAX); storage and indexes keep raw typed vectors and keys instead.
///
/// Equality between values of different types is simply false (std::variant semantics), but
/// ordering them is a logic error and throws TypeMismatchError: the engine only ever compares
/// values drawn from the same column, and the parser types every literal by its column.
class Value {
 public:
  using Variant = std::variant<std::int32_t, double, bool, std::string>;

  explicit Value(std::int32_t value) noexcept : data_(value) {}
  explicit Value(double value) noexcept : data_(value) {}
  explicit Value(bool value) noexcept : data_(value) {}
  explicit Value(std::string value) noexcept : data_(std::move(value)) {}
  /// Without this overload a string literal would convert to bool.
  explicit Value(const char* value) : data_(std::string(value)) {}

  [[nodiscard]] ColumnType type() const noexcept { return static_cast<ColumnType>(data_.index()); }

  /// The stored value; throws TypeMismatchError if the value holds a different type.
  template <class T>
  [[nodiscard]] const T& get() const;

  [[nodiscard]] const Variant& variant() const noexcept { return data_; }

  friend bool operator==(const Value& lhs, const Value& rhs) = default;
  friend std::partial_ordering operator<=>(const Value& lhs, const Value& rhs);

 private:
  Variant data_;
};

template <class T>
const T& Value::get() const {
  if (const T* value = std::get_if<T>(&data_)) {
    return *value;
  }
  throw TypeMismatchError("value of type " + std::string(columnTypeName(type())) +
                          " accessed as a different type");
}

/// Prints the value exactly as the engine's output does: ints and doubles with default stream
/// formatting, bools as true/false, strings verbatim.
std::ostream& operator<<(std::ostream& os, const Value& value);

}  // namespace sillyql
