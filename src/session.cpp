#include "sillyql/session.hpp"

#include <chrono>
#include <ostream>
#include <string>

#include "sillyql/errors.hpp"

namespace sillyql {

Session::Session(std::istream& in, std::ostream& out, std::ostream& diagnostics,
                 SessionOptions options)
    : diagnostics_(diagnostics),
      options_(options),
      writer_(out, options.quiet),
      tokens_(in),
      parser_(tokens_, catalog_),
      executor_(catalog_, writer_) {}

int Session::run() {
  using Clock = std::chrono::steady_clock;
  while (true) {
    writer_.prompt();
    if (options_.interactive) {
      writer_.flush();
    }
    try {
      const std::string keyword = tokens_.next();
      // Timing starts once the command has arrived, so time spent waiting for input is excluded.
      const Clock::time_point start = Clock::now();
      const Outcome outcome = runCommand(keyword);
      if (options_.timing && !keyword.starts_with('#')) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
        diagnostics_ << "[timing] " << keyword << ' ' << elapsed.count() << " us\n";
      }
      if (outcome == Outcome::Quit) {
        writer_.flush();
        return 0;
      }
    } catch (const InputError& error) {
      writer_.flush();
      diagnostics_ << error.what() << '\n';
      return 1;
    }
  }
}

Outcome Session::runCommand(std::string_view keyword) {
  try {
    return executor_.execute(parser_.parse(keyword));
  } catch (const SqlError& error) {
    writer_.line(error.what());
    tokens_.skipLine();
    return Outcome::Continue;
  }
}

}  // namespace sillyql
