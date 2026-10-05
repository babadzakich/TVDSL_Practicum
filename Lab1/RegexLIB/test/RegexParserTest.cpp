#include <gtest/gtest.h>

#include "TestUtil.h"
#include "Regex.h"

using namespace funnylex;
using funnylex::test::full_match;

TEST(RegexParser, Literal) {
  EXPECT_TRUE(full_match("a", "a"));
  EXPECT_FALSE(full_match("a", "b"));
  EXPECT_FALSE(full_match("a", ""));
  EXPECT_FALSE(full_match("a", "aa"));
  EXPECT_TRUE(full_match("while", "while"));
  EXPECT_FALSE(full_match("while", "whil"));
}

TEST(RegexParser, Alternation) {
  EXPECT_TRUE(full_match("a|bc", "a"));
  EXPECT_TRUE(full_match("a|bc", "bc"));
  EXPECT_FALSE(full_match("a|bc", "ab"));
  EXPECT_TRUE(full_match("0|[1-9][0-9]*", "0"));
  EXPECT_TRUE(full_match("0|[1-9][0-9]*", "120"));
  EXPECT_FALSE(full_match("0|[1-9][0-9]*", "01"));
}

TEST(RegexParser, Grouping) {
  EXPECT_TRUE(full_match("(ab)*", ""));
  EXPECT_TRUE(full_match("(ab)*", "abab"));
  EXPECT_FALSE(full_match("(ab)*", "aba"));
  EXPECT_TRUE(full_match("a(b|c)d", "acd"));
  EXPECT_TRUE(full_match("((a))", "a"));
}

TEST(RegexParser, Quantifiers) {
  EXPECT_TRUE(full_match("a*", ""));
  EXPECT_TRUE(full_match("a*", "aaaa"));
  EXPECT_TRUE(full_match("a*a", "aa"));
  EXPECT_FALSE(full_match("a+", ""));
  EXPECT_TRUE(full_match("a+a", "aa"));
  EXPECT_FALSE(full_match("a+a", "a"));
  EXPECT_TRUE(full_match("a?", ""));
  EXPECT_TRUE(full_match("a?a", "a"));
  EXPECT_FALSE(full_match("a?", "aa"));
  EXPECT_TRUE(full_match("a**", "aaa"));
  EXPECT_TRUE(full_match("(a*)*", ""));
  EXPECT_TRUE(full_match("(a|)+b", "b"));
}

TEST(RegexParser, CharClasses) {
  EXPECT_TRUE(full_match("[abc]", "b"));
  EXPECT_FALSE(full_match("[abc]", "d"));
  EXPECT_TRUE(full_match("[a-d]", "c"));
  EXPECT_FALSE(full_match("[a-d]", "e"));
  EXPECT_TRUE(full_match("[a-d]", "d"));  // верхняя граница включается
  EXPECT_TRUE(full_match("[^abc]", "d"));
  EXPECT_FALSE(full_match("[^abc]", "a"));
  EXPECT_FALSE(full_match("[^abc]", "\xC3"));  // отрицание — внутри ASCII
  EXPECT_TRUE(full_match("[-a]", "-"));
  EXPECT_TRUE(full_match("[a-]", "-"));
  EXPECT_TRUE(full_match("[]]", "]"));
  EXPECT_TRUE(full_match("[^]]", "a"));
  EXPECT_TRUE(full_match("[ \\t\\r\\n]+", " \t\r\n"));
  EXPECT_TRUE(full_match("[\\]\\\\]", "\\"));
  EXPECT_TRUE(full_match("[A-Za-z_][A-Za-z0-9_]*", "_x9"));
}

TEST(RegexParser, Escapes) {
  EXPECT_TRUE(full_match("\\.", "."));
  EXPECT_FALSE(full_match("\\.", "a"));
  EXPECT_TRUE(full_match("\\*\\+\\?\\(\\)\\[\\]\\|\\\\", "*+?()[]|\\"));
  EXPECT_TRUE(full_match("\\n", "\n"));
  EXPECT_TRUE(full_match("\\x41", "A"));
  EXPECT_TRUE(full_match("\\d+", "0123"));
  EXPECT_TRUE(full_match("\\w+", "a_Z9"));
  EXPECT_TRUE(full_match("\\s", "\t"));
  EXPECT_FALSE(full_match("\\S", " "));
}

TEST(RegexParser, DotExcludesNewline) {
  EXPECT_TRUE(full_match(".", "a"));
  EXPECT_TRUE(full_match(".", "5"));
  EXPECT_FALSE(full_match(".", ""));
  EXPECT_FALSE(full_match(".", "\n"));
  EXPECT_FALSE(full_match(".", "\x80"));
}

TEST(RegexParser, Errors) {
  EXPECT_THROW(parse_regex("[abc"), RegexError);
  EXPECT_THROW(parse_regex("(a"), RegexError);
  EXPECT_THROW(parse_regex("a)"), RegexError);
  EXPECT_THROW(parse_regex("*a"), RegexError);
  EXPECT_THROW(parse_regex("a|+"), RegexError);
  EXPECT_THROW(parse_regex("\\q"), RegexError);
  EXPECT_THROW(parse_regex("a\\"), RegexError);
  EXPECT_THROW(parse_regex("[z-a]"), RegexError);
  EXPECT_THROW(parse_regex("\\x8F"), RegexError);
  EXPECT_THROW(parse_regex("\\xG0"), RegexError);
  EXPECT_THROW(parse_regex("\xD0\xB6"), RegexError);
}

TEST(RegexParser, Nullable) {
  EXPECT_TRUE(is_nullable(*parse_regex("")));
  EXPECT_TRUE(is_nullable(*parse_regex("a*")));
  EXPECT_TRUE(is_nullable(*parse_regex("a?b*")));
  EXPECT_TRUE(is_nullable(*parse_regex("a|")));
  EXPECT_TRUE(is_nullable(*parse_regex("(a?)+")));
  EXPECT_FALSE(is_nullable(*parse_regex("a+")));
  EXPECT_FALSE(is_nullable(*parse_regex("a*b")));
  EXPECT_FALSE(is_nullable(*parse_regex("0|[1-9][0-9]*")));
}
