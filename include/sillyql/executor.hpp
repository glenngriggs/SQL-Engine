#pragma once

#include <cstdint>

#include "sillyql/catalog.hpp"
#include "sillyql/command.hpp"
#include "sillyql/result_writer.hpp"

namespace sillyql {

enum class Outcome : std::uint8_t { Continue, Quit };

/// Executes bound commands against the catalog and reports their results through a
/// ResultWriter. Recoverable failures (e.g. an unreadable CSV file) are thrown as SqlError.
class Executor {
 public:
  Executor(Catalog& catalog, ResultWriter& writer) noexcept : catalog_(catalog), writer_(writer) {}

  [[nodiscard]] Outcome execute(Command command);

 private:
  void run(const CommentCmd& cmd);
  void run(CreateCmd&& cmd);
  void run(InsertCmd&& cmd);
  void run(const PrintCmd& cmd);
  void run(const DeleteCmd& cmd);
  void run(const GenerateCmd& cmd);
  void run(const JoinCmd& cmd);
  void run(const RemoveCmd& cmd);
  void run(const AggregateCmd& cmd);
  void run(const LoadCmd& cmd);
  void run(const ExplainCmd& cmd);
  void run(const QuitCmd& cmd);

  /// Writes "<verb> <n> rows to/into <table>[ from position <a> to <b>]" for INSERT and LOAD.
  void reportAppend(std::string_view verb, std::string_view preposition, std::size_t rows,
                    RowId first, const Table& table);

  Catalog& catalog_;
  ResultWriter& writer_;
};

}  // namespace sillyql
