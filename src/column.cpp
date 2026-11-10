#include "sillyql/column.hpp"

#include <algorithm>
#include <iterator>

#include "sillyql/literal.hpp"

namespace sillyql {

static_assert(
    std::is_same_v<std::variant_alternative_t<0, Column::Storage>, std::vector<std::int32_t>>);
static_assert(std::is_same_v<std::variant_alternative_t<1, Column::Storage>, std::vector<double>>);
static_assert(
    std::is_same_v<std::variant_alternative_t<2, Column::Storage>, std::vector<BoolStorage>>);
static_assert(
    std::is_same_v<std::variant_alternative_t<3, Column::Storage>, std::vector<std::string>>);

template <StorageType S>
std::optional<S> parseElement(std::string_view text) {
  if constexpr (std::is_same_v<S, std::int32_t>) {
    return parseInt(text);
  } else if constexpr (std::is_same_v<S, double>) {
    return parseDouble(text);
  } else if constexpr (std::is_same_v<S, BoolStorage>) {
    if (const auto value = parseBool(text)) {
      return static_cast<BoolStorage>(*value);
    }
    return std::nullopt;
  } else {
    if (!isStringLiteral(text)) {
      return std::nullopt;
    }
    return std::string(text);
  }
}

template std::optional<std::int32_t> parseElement<std::int32_t>(std::string_view);
template std::optional<double> parseElement<double>(std::string_view);
template std::optional<BoolStorage> parseElement<BoolStorage>(std::string_view);
template std::optional<std::string> parseElement<std::string>(std::string_view);

std::optional<Value> parseValue(ColumnType type, std::string_view text) {
  return visitStorageType(type, [text](auto tag) -> std::optional<Value> {
    if (const auto element = parseElement<typename decltype(tag)::type>(text)) {
      return toValue(*element);
    }
    return std::nullopt;
  });
}

Column::Column(ColumnType type)
    : data_(visitStorageType(
          type, [](auto tag) -> Storage { return std::vector<typename decltype(tag)::type>{}; })) {}

std::size_t Column::size() const {
  return std::visit([](const auto& values) { return values.size(); }, data_);
}

void Column::reserve(std::size_t capacity) {
  std::visit([capacity](auto& values) { values.reserve(capacity); }, data_);
}

void Column::append(const Value& value) {
  std::visit(
      [&value](auto& values) {
        using S = ElementOf<decltype(values)>;
        values.push_back(S(storageKey<S>(value)));
      },
      data_);
}

bool Column::appendText(std::string&& text) {
  return std::visit(
      [&text](auto& values) {
        using S = ElementOf<decltype(values)>;
        if constexpr (std::is_same_v<S, std::string>) {
          if (!isStringLiteral(text)) {
            return false;
          }
          values.push_back(std::move(text));
        } else {
          const std::optional<S> element = parseElement<S>(text);
          if (!element) {
            return false;
          }
          values.push_back(*element);
        }
        return true;
      },
      data_);
}

void Column::reserveFor(const Column& other) {
  const std::size_t extra = other.size();
  std::visit(
      [extra](auto& values) {
        // A column without a buffer takes over the other one instead (see appendFrom).
        const std::size_t needed = values.size() + extra;
        if (values.capacity() != 0 && needed > values.capacity()) {
          values.reserve(std::max(needed, 2 * values.capacity()));
        }
      },
      data_);
}

void Column::appendFrom(Column&& other) {
  if (other.type() != type()) {
    throw TypeMismatchError("cannot append a " + std::string(columnTypeName(other.type())) +
                            " column to a " + std::string(columnTypeName(type())) + " column");
  }
  std::visit(
      [&other](auto& values) {
        auto& source = std::get<std::remove_reference_t<decltype(values)>>(other.data_);
        if (values.capacity() == 0) {
          values.swap(source);
        } else {
          values.insert(values.end(), std::make_move_iterator(source.begin()),
                        std::make_move_iterator(source.end()));
        }
        source.clear();
      },
      data_);
}

void Column::truncate(std::size_t size) {
  std::visit(
      [size](auto& values) {
        if (size < values.size()) {
          values.erase(values.begin() + static_cast<std::ptrdiff_t>(size), values.end());
        }
      },
      data_);
}

Value Column::valueAt(RowId row) const {
  return std::visit([row](const auto& values) { return toValue(values[row]); }, data_);
}

void Column::write(std::ostream& os, RowId row) const {
  std::visit([&os, row](const auto& values) { writeElement(os, values[row]); }, data_);
}

void Column::compact(std::span<const std::uint8_t> keep) {
  std::visit(
      [keep](auto& values) {
        std::size_t kept = 0;
        for (std::size_t row = 0; row < values.size(); ++row) {
          if (keep[row] != 0) {
            if (kept != row) {
              values[kept] = std::move(values[row]);
            }
            ++kept;
          }
        }
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(kept), values.end());
      },
      data_);
}

}  // namespace sillyql
