#pragma once

#include <iosfwd>
#include <string_view>

#include "sillyql/catalog.hpp"
#include "sillyql/executor.hpp"
#include "sillyql/parser.hpp"
#include "sillyql/result_writer.hpp"
#include "sillyql/token_stream.hpp"

namespace sillyql {

struct SessionOptions {
  bool quiet = false;        ///< Suppress result rows (summaries and errors are still printed).
  bool timing = false;       ///< Report each command's elapsed time on the diagnostic stream.
  bool interactive = false;  ///< Flush the prompt before waiting for input (a terminal).
};

/// The read-eval-print loop: prompts, parses and executes commands until QUIT.
///
/// A recoverable SqlError is printed to the output stream and the rest of its input line is
/// discarded. An InputError (input ended before QUIT, or an unreadable number) is reported on
/// the diagnostic stream and ends the session with status 1.
class Session {
 public:
  Session(std::istream& in, std::ostream& out, std::ostream& diagnostics, SessionOptions options);

  // The parser and executor refer to members of this object.
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;
  ~Session() = default;

  /// Runs to completion. Returns the process exit status: 0 after QUIT, 1 on an input error.
  [[nodiscard]] int run();

 private:
  /// Parses and executes the command introduced by `keyword`, recovering from a SqlError.
  Outcome runCommand(std::string_view keyword);

  std::ostream& diagnostics_;
  SessionOptions options_;
  ResultWriter writer_;
  Catalog catalog_;
  TokenStream tokens_;
  Parser parser_;
  Executor executor_;
};

}  // namespace sillyql
