#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "sillyql/column.hpp"
#include "sillyql/compare_op.hpp"
#include "sillyql/value.hpp"

/// Typed predicate-scan kernels over a column's contiguous storage.
namespace sillyql {

/// Appends to `out`, in ascending order, the index of every element for which `matches` holds.
///
/// Selection is branch-free: each index is written to a small block buffer unconditionally and
/// the write cursor advances by the predicate's result. The loop therefore has no
/// data-dependent branch to mispredict when selectivity is around 50%, and the buffer stays in
/// L1 while the column streams through the cache sequentially.
template <class T, class Matches>
void selectRows(std::span<const T> values, Matches matches, std::vector<RowId>& out) {
  constexpr std::size_t kBlock = 1024;
  std::array<RowId, kBlock> buffer;  // Not zeroed: every slot is written before it is read.
  for (std::size_t base = 0; base < values.size(); base += kBlock) {
    const std::size_t end = std::min(values.size(), base + kBlock);
    std::size_t count = 0;
    for (std::size_t i = base; i < end; ++i) {
      buffer[count] = static_cast<RowId>(i);
      count += static_cast<std::size_t>(matches(values[i]));
    }
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(count));
  }
}

/// Row ids (ascending) of the elements satisfying `element op key`. The switch on the operator
/// is hoisted out of the loop so each instantiation compiles to a tight typed comparison loop.
template <class T>
[[nodiscard]] std::vector<RowId> scanValues(std::span<const T> values, CompareOp op, const T& key) {
  std::vector<RowId> out;
  switch (op) {
    case CompareOp::Less:
      selectRows(values, [&key](const T& v) { return v < key; }, out);
      break;
    case CompareOp::LessEqual:
      selectRows(values, [&key](const T& v) { return v <= key; }, out);
      break;
    case CompareOp::Equal:
      selectRows(values, [&key](const T& v) { return v == key; }, out);
      break;
    case CompareOp::NotEqual:
      selectRows(values, [&key](const T& v) { return v != key; }, out);
      break;
    case CompareOp::GreaterEqual:
      selectRows(values, [&key](const T& v) { return v >= key; }, out);
      break;
    case CompareOp::Greater:
      selectRows(values, [&key](const T& v) { return v > key; }, out);
      break;
  }
  return out;
}

/// Full scan of `column` for `element op key`; `key` must have the column's type.
[[nodiscard]] std::vector<RowId> scanColumn(const Column& column, CompareOp op, const Value& key);

/// 0, 1, ..., rowCount - 1.
[[nodiscard]] std::vector<RowId> allRows(std::size_t rowCount);

}  // namespace sillyql
