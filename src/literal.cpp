#include "sillyql/literal.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <system_error>

namespace sillyql {
namespace {

bool isDigit(char c) noexcept {
  return c >= '0' && c <= '9';
}

/// Stream extraction accepts an explicit leading '+', std::from_chars does not.
std::string_view stripPlus(std::string_view text) noexcept {
  if (text.size() > 1 && text.front() == '+' && (isDigit(text[1]) || text[1] == '.')) {
    text.remove_prefix(1);
  }
  return text;
}

template <class T>
std::optional<T> fromChars(std::string_view text) noexcept {
  T value{};
  const char* const last = text.data() + text.size();
  const auto [end, error] = std::from_chars(text.data(), last, value);
  if (error != std::errc{} || end != last) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

std::optional<std::int32_t> parseInt(std::string_view text) noexcept {
  return fromChars<std::int32_t>(stripPlus(text));
}

std::optional<double> parseDouble(std::string_view text) {
  text = stripPlus(text);
  // std::from_chars also accepts "inf" and "nan"; require a digit or '.' after the sign.
  const std::size_t first = (!text.empty() && text.front() == '-') ? 1 : 0;
  if (first >= text.size() || (!isDigit(text[first]) && text[first] != '.')) {
    return std::nullopt;
  }
  double value = 0.0;
  const char* const last = text.data() + text.size();
  const auto [end, error] = std::from_chars(text.data(), last, value);
  if (end != last) {
    return std::nullopt;
  }
  if (error == std::errc::result_out_of_range) {
    // from_chars reports underflow and overflow alike. Stream extraction is built on strtod,
    // which returns a signed zero for underflow and HUGE_VAL for overflow (rejected there too).
    const double nearest = std::strtod(std::string(text).c_str(), nullptr);
    return std::isinf(nearest) ? std::nullopt : std::optional<double>(nearest);
  }
  if (error != std::errc{}) {
    return std::nullopt;
  }
  return value;
}

std::optional<bool> parseBool(std::string_view text) noexcept {
  if (text == "true") {
    return true;
  }
  if (text == "false") {
    return false;
  }
  return std::nullopt;
}

bool isStringLiteral(std::string_view text) noexcept {
  return !text.empty() && std::none_of(text.begin(), text.end(), [](char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
  });
}

std::optional<std::size_t> parseCount(std::string_view text) noexcept {
  return fromChars<std::size_t>(stripPlus(text));
}

}  // namespace sillyql
