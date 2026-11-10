#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

/// Parsing of literal tokens, shared by the command parser and the CSV loader.
///
/// Each function accepts a token only if the whole token is a value of the requested type, using
/// the same spellings that `std::istream` extraction (with std::boolalpha) accepts: an optional
/// sign, decimal digits, and for doubles a fraction and exponent. "inf"/"nan" and hexadecimal
/// forms are rejected.
namespace sillyql {

[[nodiscard]] std::optional<std::int32_t> parseInt(std::string_view text) noexcept;

/// Like stream extraction, a value too small for a double reads as (signed) zero, while a value
/// too large is rejected.
[[nodiscard]] std::optional<double> parseDouble(std::string_view text);

[[nodiscard]] std::optional<bool> parseBool(std::string_view text) noexcept;

/// Whether `text` can be stored as a string value: one non-empty token without whitespace, so
/// that it prints, and reads back, as a single token like every value the command language
/// itself can produce.
[[nodiscard]] bool isStringLiteral(std::string_view text) noexcept;

/// A non-negative count (number of columns, rows, ...).
[[nodiscard]] std::optional<std::size_t> parseCount(std::string_view text) noexcept;

}  // namespace sillyql
