#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace sillyql {

/// Comparison operator of a WHERE clause: `<column> <op> <value>`.
enum class CompareOp : std::uint8_t { Less, LessEqual, Equal, NotEqual, GreaterEqual, Greater };

/// Parses "<", "<=", "=", "!=", ">=" or ">".
[[nodiscard]] std::optional<CompareOp> parseCompareOp(std::string_view text) noexcept;

/// The operator's spelling in the command language.
[[nodiscard]] std::string_view symbol(CompareOp op) noexcept;

}  // namespace sillyql
