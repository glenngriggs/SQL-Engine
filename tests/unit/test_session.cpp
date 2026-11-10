#include <gtest/gtest.h>

#include <chrono>
#include <istream>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#include "sillyql/session.hpp"
#include "test_helpers.hpp"

namespace sillyql {
namespace {

using test::runScript;

struct TimingLine {
  std::string command;
  long long micros = -1;
};

/// Parses "[timing] <command> <micros> us" lines, checking their format.
std::vector<TimingLine> timingLines(const std::string& diagnostics) {
  std::istringstream lines(diagnostics);
  std::vector<TimingLine> parsed;
  for (std::string line; std::getline(lines, line);) {
    std::istringstream fields(line);
    std::string tag;
    std::string unit;
    TimingLine timing;
    fields >> tag >> timing.command >> timing.micros >> unit;
    EXPECT_EQ(tag, "[timing]") << line;
    EXPECT_GE(timing.micros, 0) << line;
    EXPECT_EQ(unit, "us") << line;
    parsed.push_back(timing);
  }
  return parsed;
}

std::vector<std::string> commandsOf(const std::vector<TimingLine>& lines) {
  std::vector<std::string> commands;
  commands.reserve(lines.size());
  for (const TimingLine& line : lines) {
    commands.push_back(line.command);
  }
  return commands;
}

/// An input stream buffer that hands out one line at a time and waits `delay` before each,
/// like a person typing commands.
class SlowLineBuffer : public std::streambuf {
 public:
  SlowLineBuffer(std::vector<std::string> lines, std::chrono::milliseconds delay)
      : lines_(std::move(lines)), delay_(delay) {}

 protected:
  int_type underflow() override {
    if (next_ == lines_.size()) {
      return traits_type::eof();
    }
    std::this_thread::sleep_for(delay_);
    current_ = lines_[next_++] + '\n';
    setg(current_.data(), current_.data(), current_.data() + current_.size());
    return traits_type::to_int_type(current_.front());
  }

 private:
  std::vector<std::string> lines_;
  std::chrono::milliseconds delay_;
  std::size_t next_ = 0;
  std::string current_;
};

TEST(SessionTest, EndOfInputBeforeQuitFailsWithStatusOne) {
  const auto result = runScript("CREATE t 1 int a\n");
  EXPECT_EQ(result.exitCode, 1);
  EXPECT_EQ(result.out, "% New table t with column(s) a created\n% ");
  EXPECT_EQ(result.diagnostics, "Error: Reading from cin has failed\n");
}

TEST(SessionTest, MalformedNumberFailsWithStatusOne) {
  const auto result = runScript("CREATE t 1 int a\nINSERT INTO t 1 ROWS\nabc\nQUIT\n");
  EXPECT_EQ(result.exitCode, 1);
  EXPECT_EQ(result.diagnostics, "Error: Reading from cin has failed\n");
}

TEST(SessionTest, UnderflowingDoubleReadsAsZeroLikeTheOriginal) {
  const auto result =
      runScript("CREATE t 1 double a\nINSERT INTO t 1 ROWS\n1e-400\nPRINT FROM t 1 a ALL\nQUIT\n");
  EXPECT_EQ(result.exitCode, 0);
  EXPECT_NE(result.out.find("% a \n0 \nPrinted 1 matching rows from t\n"), std::string::npos)
      << result.out;
}

TEST(SessionTest, ErrorRecoverySkipsOnlyTheLineTheHeaderEndsOn) {
  const auto result = runScript(
      "CREATE t 1 int a\n"
      "DELETE FROM nope\nWHERE a = 1\n"
      "JOIN t AND nope\nWHERE a = a AND PRINT 1 a 1\n"
      "QUIT\n");
  EXPECT_EQ(result.out,
            "% New table t with column(s) a created\n"
            "% Error during DELETE: nope does not name a table in the database\n"
            "% Error during JOIN: nope does not name a table in the database\n"
            "% Thanks for being silly!\n");
}

TEST(SessionTest, TimingGoesToDiagnosticsOnlyAndCoversFailedCommands) {
  const std::string script =
      "CREATE t 1 int a\n# note\nPRINT FROM nope 1 a ALL\nFOO\nPRINT FROM t 1 a ALL\nQUIT\n";
  const auto plain = runScript(script);
  const auto timed = runScript(script, {.timing = true});
  EXPECT_EQ(timed.exitCode, 0);
  EXPECT_EQ(timed.out, plain.out);
  EXPECT_TRUE(plain.diagnostics.empty());
  EXPECT_EQ(commandsOf(timingLines(timed.diagnostics)),
            (std::vector<std::string>{"CREATE", "PRINT", "FOO", "PRINT", "QUIT"}));
}

TEST(SessionTest, TimingExcludesTimeSpentWaitingForInput) {
  constexpr std::chrono::milliseconds kTypingDelay(100);
  SlowLineBuffer slow({"CREATE t 1 int a", "PRINT FROM t 1 a ALL", "QUIT"}, kTypingDelay);
  std::istream in(&slow);
  std::ostringstream out;
  std::ostringstream diagnostics;
  Session session(in, out, diagnostics, {.timing = true});
  ASSERT_EQ(session.run(), 0);
  const std::vector<TimingLine> lines = timingLines(diagnostics.str());
  ASSERT_EQ(lines.size(), 3U);
  for (const TimingLine& line : lines) {
    EXPECT_LT(line.micros, std::chrono::microseconds(kTypingDelay).count() / 2) << line.command;
  }
}

}  // namespace
}  // namespace sillyql
