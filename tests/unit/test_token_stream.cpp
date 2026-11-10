#include <gtest/gtest.h>

#include <sstream>

#include "sillyql/errors.hpp"
#include "sillyql/token_stream.hpp"

namespace sillyql {
namespace {

TEST(TokenStreamTest, SplitsOnAnyWhitespaceAcrossLines) {
  std::istringstream in("CREATE  t\t2\n  int   string\n");
  TokenStream tokens(in);
  EXPECT_EQ(tokens.next(), "CREATE");
  EXPECT_EQ(tokens.next(), "t");
  EXPECT_EQ(tokens.next(), "2");
  EXPECT_EQ(tokens.next(), "int");
  EXPECT_EQ(tokens.next(), "string");
  EXPECT_FALSE(tokens.tryNext());
}

TEST(TokenStreamTest, NextThrowsInputErrorAtEndOfInput) {
  std::istringstream in("   \n");
  TokenStream tokens(in);
  EXPECT_THROW((void)tokens.next(), InputError);
}

TEST(TokenStreamTest, SkipLineDiscardsTheRestOfTheLine) {
  std::istringstream in("REMOVE ghost trailing words\nQUIT\n");
  TokenStream tokens(in);
  EXPECT_EQ(tokens.next(), "REMOVE");
  EXPECT_EQ(tokens.next(), "ghost");
  tokens.skipLine();
  EXPECT_EQ(tokens.next(), "QUIT");
}

TEST(TokenStreamTest, SkipLineAfterLastTokenOnlyConsumesTheNewline) {
  std::istringstream in("REMOVE ghost\nQUIT\n");
  TokenStream tokens(in);
  (void)tokens.next();
  (void)tokens.next();
  tokens.skipLine();
  EXPECT_EQ(tokens.next(), "QUIT");
}

TEST(TokenStreamTest, SkipLineAtEndOfInputIsHarmless) {
  std::istringstream in("QUIT");
  TokenStream tokens(in);
  (void)tokens.next();
  tokens.skipLine();
  EXPECT_FALSE(tokens.tryNext());
}

TEST(TokenStreamTest, HasMoreOnLineLooksOnlyAtTheCurrentLine) {
  std::istringstream in("AGGREGATE x   GROUP\nnext\r\n  \nlast");
  TokenStream tokens(in);
  (void)tokens.next();
  (void)tokens.next();
  EXPECT_TRUE(tokens.hasMoreOnLine());
  EXPECT_EQ(tokens.next(), "GROUP");
  EXPECT_FALSE(tokens.hasMoreOnLine());
  EXPECT_EQ(tokens.next(), "next");
  EXPECT_FALSE(tokens.hasMoreOnLine());  // A trailing '\r' is whitespace.
  EXPECT_EQ(tokens.next(), "last");
  EXPECT_FALSE(tokens.hasMoreOnLine());  // End of input.
}

TEST(TokenStreamTest, HasMoreOnLineTreatsACommentAsTheEndOfTheLine) {
  std::istringstream in("COUNT x   # note\nnext");
  TokenStream tokens(in);
  (void)tokens.next();
  (void)tokens.next();
  EXPECT_FALSE(tokens.hasMoreOnLine());
  EXPECT_EQ(tokens.next(), "#");  // Left for the next command.
}

}  // namespace
}  // namespace sillyql
