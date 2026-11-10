#pragma once

#include <iosfwd>
#include <optional>
#include <string>

namespace sillyql {

/// Splits the command input into whitespace-separated tokens.
///
/// The command language is token based (a command may span lines, e.g. INSERT rows) except for
/// two line-oriented rules that this class also supports: error recovery discards the rest of
/// the current line, and AGGREGATE's optional clauses must appear on the command's own line.
class TokenStream {
 public:
  explicit TokenStream(std::istream& in) noexcept : in_(in) {}

  /// The next token, or nullopt at end of input.
  [[nodiscard]] std::optional<std::string> tryNext();

  /// The next token; throws InputError at end of input.
  [[nodiscard]] std::string next();

  /// Whether another token starts before the end of the current line. A token starting with '#'
  /// begins a comment, so it does not count; it is left for the next command, as after any other
  /// command. Consumes only the blanks in front of the token.
  [[nodiscard]] bool hasMoreOnLine();

  /// Discards everything up to and including the next newline.
  void skipLine();

 private:
  std::istream& in_;
};

}  // namespace sillyql
