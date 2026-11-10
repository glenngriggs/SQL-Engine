#include <gtest/gtest.h>

#include <sstream>
#include <string_view>
#include <vector>

#include "sillyql/cli_options.hpp"
#include "sillyql/errors.hpp"

namespace sillyql {
namespace {

CliOptions parse(std::vector<std::string_view> args) {
  return parseCommandLine(args);
}

TEST(CliOptionsTest, DefaultsToNoFlags) {
  const CliOptions options = parse({});
  EXPECT_FALSE(options.help);
  EXPECT_FALSE(options.quiet);
  EXPECT_FALSE(options.timing);
}

TEST(CliOptionsTest, AcceptsShortAndLongFlags) {
  EXPECT_TRUE(parse({"-q"}).quiet);
  EXPECT_TRUE(parse({"--quiet"}).quiet);
  EXPECT_TRUE(parse({"-t"}).timing);
  EXPECT_TRUE(parse({"--timing"}).timing);
  EXPECT_TRUE(parse({"-h"}).help);
  EXPECT_TRUE(parse({"--help"}).help);
}

TEST(CliOptionsTest, AcceptsCombinedShortFlags) {
  const CliOptions options = parse({"-qt"});
  EXPECT_TRUE(options.quiet);
  EXPECT_TRUE(options.timing);
  EXPECT_FALSE(options.help);
}

TEST(CliOptionsTest, AcceptsUnambiguousLongOptionPrefixes) {
  EXPECT_TRUE(parse({"--qui"}).quiet);
  EXPECT_TRUE(parse({"--t"}).timing);
  EXPECT_TRUE(parse({"--he"}).help);
  EXPECT_THROW(parse({"--quiets"}), UsageError);
}

TEST(CliOptionsTest, DoubleDashEndsTheOptions) {
  EXPECT_TRUE(parse({"-q", "--"}).quiet);
  EXPECT_FALSE(parse({"--"}).quiet);
  EXPECT_THROW(parse({"--", "-q"}), UsageError);  // Positional after "--".
}

TEST(CliOptionsTest, HelpStopsParsingLikeGetopt) {
  EXPECT_TRUE(parse({"-h", "-x"}).help);
  EXPECT_TRUE(parse({"-hx"}).help);
  EXPECT_TRUE(parse({"--help", "extra"}).help);
  EXPECT_THROW(parse({"-x", "-h"}), UsageError);
}

TEST(CliOptionsTest, RejectsUnknownArguments) {
  EXPECT_THROW(parse({"-x"}), UsageError);
  EXPECT_THROW(parse({"-qx"}), UsageError);
  EXPECT_THROW(parse({"--verbose"}), UsageError);
  EXPECT_THROW(parse({"input.sql"}), UsageError);
  EXPECT_THROW(parse({"-"}), UsageError);
}

TEST(CliOptionsTest, UsageErrorUsesTheDocumentedMessage) {
  try {
    parse({"-x"});
    FAIL() << "expected UsageError";
  } catch (const UsageError& error) {
    EXPECT_STREQ(error.what(), "Error: Unknown command line option");
  }
}

TEST(CliOptionsTest, UsageNamesTheProgramAndEveryFlag) {
  std::ostringstream os;
  printUsage(os, "./silly");
  const std::string usage = os.str();
  EXPECT_EQ(usage.rfind("Usage: ./silly [options] < inputfile\n", 0), 0U);
  for (const char* flag : {"--help", "--quiet", "--timing", "-h", "-q", "-t"}) {
    EXPECT_NE(usage.find(flag), std::string::npos) << flag;
  }
}

}  // namespace
}  // namespace sillyql
