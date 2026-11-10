#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace sillyql {

struct CliOptions {
  bool help = false;
  bool quiet = false;
  bool timing = false;
};

/// Parses the arguments after the program name with getopt_long's conventions: -h/--help,
/// -q/--quiet, -t/--timing; short flags may be combined ("-qt"), a long option may be
/// abbreviated to any unambiguous prefix ("--qui"), and "--" ends the options. Parsing stops at
/// the first help flag, as the original did. Throws UsageError for an unknown option and for any
/// positional argument (the program reads only standard input).
[[nodiscard]] CliOptions parseCommandLine(std::span<const std::string_view> args);

void printUsage(std::ostream& os, std::string_view program);

}  // namespace sillyql
