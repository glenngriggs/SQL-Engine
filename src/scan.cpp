#include "sillyql/scan.hpp"

#include <numeric>

namespace sillyql {

std::vector<RowId> scanColumn(const Column& column, CompareOp op, const Value& key) {
  return column.visit([op, &key](const auto& values) {
    using S = ElementOf<decltype(values)>;
    const S& typedKey = storageKey<S>(key);
    return scanValues(std::span<const S>(values), op, typedKey);
  });
}

std::vector<RowId> allRows(std::size_t rowCount) {
  std::vector<RowId> rows(rowCount);
  std::iota(rows.begin(), rows.end(), RowId{0});
  return rows;
}

}  // namespace sillyql
