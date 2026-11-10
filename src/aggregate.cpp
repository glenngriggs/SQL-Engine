#include "sillyql/aggregate.hpp"

#include <algorithm>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace sillyql {
namespace {

/// Running state of one group. Only what the column type supports is accumulated: integer sums
/// in 64 bits (exact for any realistic row count), double sums with compensation.
template <StorageType S>
class Accumulator {
 public:
  void add(const S& value) {
    ++count_;
    if constexpr (std::is_same_v<S, std::int32_t>) {
      intSum_ += value;
    } else if constexpr (std::is_same_v<S, double>) {
      doubleSum_.add(value);
    }
    if (!min_ || value < *min_) {
      min_ = value;
    }
    if (!max_ || *max_ < value) {
      max_ = value;
    }
  }

  [[nodiscard]] AggregateValue result(AggregateFunction function) const {
    if (function == AggregateFunction::Count) {
      return static_cast<std::int64_t>(count_);
    }
    if (count_ == 0) {
      return std::monostate{};
    }
    switch (function) {
      case AggregateFunction::Sum:
        return sum();
      case AggregateFunction::Avg:
        return average();
      case AggregateFunction::Min:
        return toValue(*min_);
      case AggregateFunction::Max:
        return toValue(*max_);
      case AggregateFunction::Count:
        break;
    }
    return std::monostate{};
  }

 private:
  [[nodiscard]] AggregateValue sum() const {
    if constexpr (std::is_same_v<S, std::int32_t>) {
      return intSum_;
    } else if constexpr (std::is_same_v<S, double>) {
      return doubleSum_.value();
    } else {
      throw std::logic_error("SUM of a non-numeric column");
    }
  }

  [[nodiscard]] AggregateValue average() const {
    const auto count = static_cast<double>(count_);
    if constexpr (std::is_same_v<S, std::int32_t>) {
      return static_cast<double>(intSum_) / count;
    } else if constexpr (std::is_same_v<S, double>) {
      return doubleSum_.value() / count;
    } else {
      throw std::logic_error("AVG of a non-numeric column");
    }
  }

  std::size_t count_ = 0;
  std::int64_t intSum_ = 0;
  CompensatedSum doubleSum_;
  std::optional<S> min_;
  std::optional<S> max_;
};

}  // namespace

std::optional<AggregateFunction> parseAggregateFunction(std::string_view name) noexcept {
  if (name == "COUNT") {
    return AggregateFunction::Count;
  }
  if (name == "SUM") {
    return AggregateFunction::Sum;
  }
  if (name == "AVG") {
    return AggregateFunction::Avg;
  }
  if (name == "MIN") {
    return AggregateFunction::Min;
  }
  if (name == "MAX") {
    return AggregateFunction::Max;
  }
  return std::nullopt;
}

std::string_view aggregateFunctionName(AggregateFunction function) noexcept {
  switch (function) {
    case AggregateFunction::Count:
      return "COUNT";
    case AggregateFunction::Sum:
      return "SUM";
    case AggregateFunction::Avg:
      return "AVG";
    case AggregateFunction::Min:
      return "MIN";
    case AggregateFunction::Max:
      return "MAX";
  }
  return "?";
}

bool acceptsColumnType(AggregateFunction function, ColumnType type) noexcept {
  const bool numeric = type == ColumnType::Int || type == ColumnType::Double;
  return numeric || (function != AggregateFunction::Sum && function != AggregateFunction::Avg);
}

void writeAggregateValue(std::ostream& os, const AggregateValue& value) {
  std::visit(
      [&os](const auto& v) {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::monostate>) {
          os << "NULL";
        } else {
          os << v;
        }
      },
      value);
}

AggregateResult aggregate(const Table& table, const AggregateQuery& query,
                          std::vector<RowId> rows) {
  if (!std::is_sorted(rows.begin(), rows.end())) {
    std::sort(rows.begin(), rows.end());
  }
  AggregateResult result;
  result.inputRows = rows.size();

  table.column(query.column).visit([&](const auto& values) {
    using S = ElementOf<decltype(values)>;
    if (!query.groupBy) {
      Accumulator<S> total;
      for (const RowId row : rows) {
        total.add(values[row]);
      }
      result.groups.push_back({std::nullopt, total.result(query.function)});
      return;
    }
    table.column(*query.groupBy).visit([&](const auto& keys) {
      using K = ElementOf<decltype(keys)>;
      // Accumulate in a hash map (O(1) per row), then sort only the distinct groups.
      std::unordered_map<K, Accumulator<S>> groups;
      for (const RowId row : rows) {
        groups[keys[row]].add(values[row]);
      }
      std::vector<std::pair<K, Accumulator<S>>> ordered(std::make_move_iterator(groups.begin()),
                                                        std::make_move_iterator(groups.end()));
      std::sort(ordered.begin(), ordered.end(),
                [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
      result.groups.reserve(ordered.size());
      for (const auto& [key, accumulator] : ordered) {
        result.groups.push_back({toValue(key), accumulator.result(query.function)});
      }
    });
  });
  return result;
}

}  // namespace sillyql
