#pragma once

#include <cstddef>

#include "sillyql/compare_op.hpp"
#include "sillyql/value.hpp"

namespace sillyql {

/// A WHERE clause `<column> <op> <value>`, bound to a column position. The parser guarantees
/// that `value` has the column's type.
struct Predicate {
  std::size_t column;
  CompareOp op;
  Value value;

  friend bool operator==(const Predicate&, const Predicate&) = default;
};

}  // namespace sillyql
