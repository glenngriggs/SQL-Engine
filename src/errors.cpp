#include "sillyql/errors.hpp"

#include <initializer_list>
#include <string>

namespace sillyql {
namespace {

std::string during(std::string_view verb, std::string_view detail) {
  std::string message = "Error during ";
  message.append(verb).append(": ").append(detail);
  return message;
}

std::string concat(std::initializer_list<std::string_view> parts) {
  std::string text;
  for (const std::string_view part : parts) {
    text.append(part);
  }
  return text;
}

}  // namespace

UnknownCommandError::UnknownCommandError() : SqlError("Error: unrecognized command") {}

TableNotFoundError::TableNotFoundError(std::string_view verb, std::string_view table)
    : SqlError(during(verb, concat({table, " does not name a table in the database"}))) {}

ColumnNotFoundError::ColumnNotFoundError(std::string_view verb, std::string_view column,
                                         std::string_view table)
    : SqlError(during(verb, concat({column, " does not name a column in ", table}))) {}

TableExistsError::TableExistsError(std::string_view table)
    : SqlError(during("CREATE", concat({"Cannot create already existing table ", table}))) {}

SyntaxError::SyntaxError(std::string_view verb, std::string_view detail)
    : SqlError(during(verb, detail)) {}

SemanticError::SemanticError(std::string_view verb, std::string_view detail)
    : SqlError(during(verb, detail)) {}

LoadError::LoadError(std::string_view detail) : SqlError(during("LOAD", detail)) {}

InputError::InputError() : std::runtime_error("Error: Reading from cin has failed") {}

UsageError::UsageError() : std::runtime_error("Error: Unknown command line option") {}

}  // namespace sillyql
