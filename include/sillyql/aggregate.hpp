#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "sillyql/table.hpp"
#include "sillyql/value.hpp"

namespace sillyql {

enum class AggregateFunction : std::uint8_t { Count, Sum, Avg, Min, Max };

/// Parses "COUNT", "SUM", "AVG", "MIN" or "MAX".
[[nodiscard]] std::optional<AggregateFunction> parseAggregateFunction(
    std::string_view name) noexcept;
[[nodiscard]] std::string_view aggregateFunctionName(AggregateFunction function) noexcept;

/// SUM and AVG accept only int and double columns.
[[nodiscard]] bool acceptsColumnType(AggregateFunction function, ColumnType type) noexcept;

/// `FUNC(column) [GROUP BY groupBy]`, with columns bound to positions.
struct AggregateQuery {
  AggregateFunction function;
  std::size_t column;
  std::optional<std::size_t> groupBy;

  friend bool operator==(const AggregateQuery&, const AggregateQuery&) = default;
};

/// An aggregate's result: NULL (SUM/AVG/MIN/MAX of no rows), an exact integer (COUNT, SUM of an
/// int column), a double (SUM of a double column, AVG) or a column value (MIN, MAX).
using AggregateValue = std::variant<std::monostate, std::int64_t, double, Value>;

/// Writes the value with the engine's output formatting; NULL is written as "NULL".
void writeAggregateValue(std::ostream& os, const AggregateValue& value);

struct AggregateGroup {
  std::optional<Value> key;  ///< nullopt without GROUP BY.
  AggregateValue value;
};

struct AggregateResult {
  std::vector<AggregateGroup> groups;  ///< In ascending key order.
  std::size_t inputRows = 0;
};

/// Computes `query` over `rows` of `table`. Without GROUP BY the result has exactly one group,
/// even for no input rows (COUNT is then 0 and the other functions NULL). Rows are accumulated in
/// ascending row order whatever order they are given in (only an ordered range scan delivers
/// them otherwise), so floating-point results do not depend on the access path that selected
/// them.
[[nodiscard]] AggregateResult aggregate(const Table& table, const AggregateQuery& query,
                                        std::vector<RowId> rows);

/// Neumaier's compensated summation: the rounding error of each addition is carried in a
/// separate term, so the error of the total stays O(epsilon) instead of growing with the number
/// of terms.
class CompensatedSum {
 public:
  void add(double term) noexcept {
    const double total = sum_ + term;
    if (std::abs(sum_) >= std::abs(term)) {
      compensation_ += (sum_ - total) + term;
    } else {
      compensation_ += (term - total) + sum_;
    }
    sum_ = total;
  }

  [[nodiscard]] double value() const noexcept {
    // Once the running sum overflows, inf - inf would turn the compensation into NaN.
    return std::isfinite(sum_) ? sum_ + compensation_ : sum_;
  }

 private:
  double sum_ = 0.0;
  double compensation_ = 0.0;
};

}  // namespace sillyql
