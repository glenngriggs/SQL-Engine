#include "sillyql/compare_op.hpp"

namespace sillyql {

std::optional<CompareOp> parseCompareOp(std::string_view text) noexcept {
  if (text == "<") {
    return CompareOp::Less;
  }
  if (text == "<=") {
    return CompareOp::LessEqual;
  }
  if (text == "=") {
    return CompareOp::Equal;
  }
  if (text == "!=") {
    return CompareOp::NotEqual;
  }
  if (text == ">=") {
    return CompareOp::GreaterEqual;
  }
  if (text == ">") {
    return CompareOp::Greater;
  }
  return std::nullopt;
}

std::string_view symbol(CompareOp op) noexcept {
  switch (op) {
    case CompareOp::Less:
      return "<";
    case CompareOp::LessEqual:
      return "<=";
    case CompareOp::Equal:
      return "=";
    case CompareOp::NotEqual:
      return "!=";
    case CompareOp::GreaterEqual:
      return ">=";
    case CompareOp::Greater:
      return ">";
  }
  return "?";
}

}  // namespace sillyql
