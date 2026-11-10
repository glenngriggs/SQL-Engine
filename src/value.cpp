#include "sillyql/value.hpp"

#include <ostream>
#include <type_traits>

namespace sillyql {

static_assert(std::is_same_v<std::variant_alternative_t<0, Value::Variant>, std::int32_t>);
static_assert(std::is_same_v<std::variant_alternative_t<1, Value::Variant>, double>);
static_assert(std::is_same_v<std::variant_alternative_t<2, Value::Variant>, bool>);
static_assert(std::is_same_v<std::variant_alternative_t<3, Value::Variant>, std::string>);

std::string_view columnTypeName(ColumnType type) noexcept {
  switch (type) {
    case ColumnType::Int:
      return "int";
    case ColumnType::Double:
      return "double";
    case ColumnType::Bool:
      return "bool";
    case ColumnType::String:
      return "string";
  }
  return "unknown";
}

std::optional<ColumnType> parseColumnType(std::string_view name) noexcept {
  if (name == "int") {
    return ColumnType::Int;
  }
  if (name == "double") {
    return ColumnType::Double;
  }
  if (name == "bool") {
    return ColumnType::Bool;
  }
  if (name == "string") {
    return ColumnType::String;
  }
  return std::nullopt;
}

std::partial_ordering operator<=>(const Value& lhs, const Value& rhs) {
  if (lhs.type() != rhs.type()) {
    throw TypeMismatchError("cannot order a " + std::string(columnTypeName(lhs.type())) +
                            " value against a " + std::string(columnTypeName(rhs.type())) +
                            " value");
  }
  return std::visit(
      [&rhs](const auto& left) -> std::partial_ordering {
        using T = std::decay_t<decltype(left)>;
        return left <=> std::get<T>(rhs.data_);
      },
      lhs.data_);
}

std::ostream& operator<<(std::ostream& os, const Value& value) {
  std::visit(
      [&os](const auto& v) {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
          os << (v ? "true" : "false");
        } else {
          os << v;
        }
      },
      value.variant());
  return os;
}

}  // namespace sillyql
