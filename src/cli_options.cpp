#include "sillyql/cli_options.hpp"

#include <array>
#include <ostream>

#include "sillyql/errors.hpp"

namespace sillyql {
namespace {

struct Flag {
  char shortName;
  std::string_view longName;
};

constexpr std::array<Flag, 3> kFlags{{{'h', "help"}, {'q', "quiet"}, {'t', "timing"}}};

/// Sets the option named by `flag`; returns true if it was help, which ends parsing.
bool apply(char flag, CliOptions& options) {
  switch (flag) {
    case 'h':
      options.help = true;
      return true;
    case 'q':
      options.quiet = true;
      return false;
    case 't':
      options.timing = true;
      return false;
    default:
      throw UsageError();
  }
}

/// The short flag of the long option `name` names, which may be an unambiguous prefix.
char resolveLong(std::string_view name) {
  char match = 0;
  for (const Flag& flag : kFlags) {
    if (flag.longName == name) {
      return flag.shortName;
    }
    if (!name.empty() && flag.longName.starts_with(name)) {
      if (match != 0) {
        throw UsageError();  // Ambiguous prefix.
      }
      match = flag.shortName;
    }
  }
  if (match == 0) {
    throw UsageError();
  }
  return match;
}

}  // namespace

CliOptions parseCommandLine(std::span<const std::string_view> args) {
  CliOptions options;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (arg == "--") {
      if (i + 1 < args.size()) {
        throw UsageError();  // Only positional arguments may follow.
      }
      break;
    }
    if (arg.starts_with("--")) {
      if (apply(resolveLong(arg.substr(2)), options)) {
        return options;
      }
    } else if (arg.size() >= 2 && arg[0] == '-') {
      for (const char flag : arg.substr(1)) {
        if (apply(flag, options)) {
          return options;
        }
      }
    } else {
      throw UsageError();
    }
  }
  return options;
}

void printUsage(std::ostream& os, std::string_view program) {
  os << "Usage: " << program << " [options] < inputfile\n"
     << "Options:\n"
     << "  -h, --help     Show this help message and exit\n"
     << "  -q, --quiet    Suppress result rows (summaries and errors are still printed)\n"
     << "  -t, --timing   Print each command's elapsed time in microseconds to stderr\n";
}

}  // namespace sillyql
