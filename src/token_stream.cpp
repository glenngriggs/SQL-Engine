#include "sillyql/token_stream.hpp"

#include <cctype>
#include <istream>
#include <limits>
#include <utility>

#include "sillyql/errors.hpp"

namespace sillyql {

std::optional<std::string> TokenStream::tryNext() {
  std::string token;
  if (in_ >> token) {
    return token;
  }
  return std::nullopt;
}

std::string TokenStream::next() {
  auto token = tryNext();
  if (!token) {
    throw InputError();
  }
  return std::move(*token);
}

bool TokenStream::hasMoreOnLine() {
  using Traits = std::istream::traits_type;
  while (true) {
    const Traits::int_type c = in_.peek();
    if (Traits::eq_int_type(c, Traits::eof()) || Traits::to_char_type(c) == '\n') {
      return false;
    }
    const char next = Traits::to_char_type(c);
    if (std::isspace(static_cast<unsigned char>(next)) == 0) {
      return next != '#';
    }
    in_.get();
  }
}

void TokenStream::skipLine() {
  in_.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

}  // namespace sillyql
