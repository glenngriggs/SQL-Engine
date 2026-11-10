#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <string_view>
#include <unistd.h>
#include <vector>

#include "sillyql/cli_options.hpp"
#include "sillyql/errors.hpp"
#include "sillyql/session.hpp"

int main(int argc, char** argv) {
  std::ios_base::sync_with_stdio(false);
  // Untied, reading a token no longer flushes std::cout; the session flushes the prompt itself
  // when a person is typing.
  std::cin.tie(nullptr);

  const std::span<char*> argSpan(argv, static_cast<std::size_t>(argc));
  const std::string_view program = argSpan.empty() ? "silly" : argSpan.front();
  const std::span<char*> rest = argSpan.empty() ? argSpan : argSpan.subspan(1);
  const std::vector<std::string_view> args(rest.begin(), rest.end());
  try {
    const sillyql::CliOptions options = sillyql::parseCommandLine(args);
    if (options.help) {
      sillyql::printUsage(std::cout, program);
      return EXIT_SUCCESS;
    }
    sillyql::Session session(std::cin, std::cout, std::cerr,
                             {.quiet = options.quiet,
                              .timing = options.timing,
                              .interactive = isatty(STDIN_FILENO) != 0});
    return session.run();
  } catch (const sillyql::UsageError& error) {
    std::cerr << error.what() << '\n';
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
  }
  return EXIT_FAILURE;
}
