#pragma once

#include <stdexcept>
#include <string_view>

namespace sillyql {

/// Base of every recoverable command error. what() is the complete line shown to the user; the
/// session prints it to stdout, discards the rest of the input line and reads the next command.
class SqlError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// The first token of a command is not a known command keyword.
class UnknownCommandError final : public SqlError {
 public:
  UnknownCommandError();
};

class TableNotFoundError final : public SqlError {
 public:
  TableNotFoundError(std::string_view verb, std::string_view table);
};

class ColumnNotFoundError final : public SqlError {
 public:
  ColumnNotFoundError(std::string_view verb, std::string_view column, std::string_view table);
};

class TableExistsError final : public SqlError {
 public:
  explicit TableExistsError(std::string_view table);
};

/// The command is structurally malformed (unexpected keyword, unknown operator, ...).
class SyntaxError final : public SqlError {
 public:
  SyntaxError(std::string_view verb, std::string_view detail);
};

/// The command is well formed but invalid for the schema (bad column type, type mismatch, ...).
class SemanticError final : public SqlError {
 public:
  SemanticError(std::string_view verb, std::string_view detail);
};

/// A CSV file could not be loaded; the target table is left unchanged.
class LoadError final : public SqlError {
 public:
  explicit LoadError(std::string_view detail);
};

/// Unrecoverable failure of the command stream: input ended before QUIT, or a token could not be
/// read as the number or value the grammar requires. Ends the session with exit status 1.
class InputError final : public std::runtime_error {
 public:
  InputError();
};

/// Invalid command-line arguments.
class UsageError final : public std::runtime_error {
 public:
  UsageError();
};

/// Programming error: values of different types were ordered against each other, or a value was
/// read as the wrong type. User input cannot trigger it because the parser type-checks literals.
class TypeMismatchError final : public std::logic_error {
 public:
  using std::logic_error::logic_error;
};

}  // namespace sillyql
