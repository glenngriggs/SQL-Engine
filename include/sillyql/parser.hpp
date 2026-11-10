#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "sillyql/catalog.hpp"
#include "sillyql/command.hpp"
#include "sillyql/token_stream.hpp"

namespace sillyql {

/// Reads one command at a time from a TokenStream and binds it against the catalog.
///
/// Parsing and binding are deliberately done together. Literal syntax is type directed (the
/// meaning of a token depends on its column's type), and INSERT needs the table's width to know
/// how many tokens a row has. Each error must also be detected after exactly the tokens the
/// legacy engine read, because recovery discards only the rest of the current line: a command's
/// fixed-length header (e.g. `FROM <table> WHERE <col> <op>` for DELETE) is read in full before
/// any of it is checked, while variable-length parts (column lists, values) are checked as they
/// are read. Within a header, keywords are checked first, then table names, then column names,
/// then operators and counts.
///
/// Errors are thrown: SqlError subclasses for recoverable problems, InputError when the input
/// ends or a number cannot be read.
class Parser {
 public:
  Parser(TokenStream& tokens, const Catalog& catalog) noexcept
      : tokens_(tokens), catalog_(catalog) {}

  /// Parses the command introduced by `keyword`, its first token, which the caller has already
  /// read (the Session times a command from the moment its keyword arrives).
  [[nodiscard]] Command parse(std::string_view keyword);

 private:
  CreateCmd parseCreate();
  InsertCmd parseInsert();
  PrintCmd parsePrint();
  DeleteCmd parseDelete();
  GenerateCmd parseGenerate();
  JoinCmd parseJoin();
  RemoveCmd parseRemove();
  AggregateCmd parseAggregate();
  LoadCmd parseLoad();
  ExplainCmd parseExplain();

  /// The next N tokens, unchecked.
  template <std::size_t N>
  std::array<std::string, N> readHeader();

  Predicate readPredicate(std::string_view verb, const Table& table);
  /// Binds `column op <value>`; the value token is read only after column and operator check out.
  Predicate bindPredicate(std::string_view verb, const Table& table, std::string_view column,
                          std::string_view op);

  TokenStream& tokens_;
  const Catalog& catalog_;
};

}  // namespace sillyql
