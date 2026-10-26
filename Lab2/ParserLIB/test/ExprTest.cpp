#include <gtest/gtest.h>

#include <string>

#include "AstPrinter.h"
#include "Parser.h"
#include "SExpr.h"

using namespace funny;

namespace {

std::string sx(std::string_view src) {
  auto r = parse_expression(src);
  EXPECT_TRUE(r.diagnostics.empty()) << src;
  return r.node ? sexpr(*r.node) : "<null>";
}

std::string err(std::string_view src) {
  auto r = parse_expression(src);
  std::string out;
  for (const Diagnostic& d : r.diagnostics) {
    out += (out.empty() ? "" : "\n") + format_diagnostic("t", d);
  }
  return out;
}

}  // namespace

TEST(Expr, LeftAssociative) {
  EXPECT_EQ(sx("1 - 2 - 3"), "(- (- 1 2) 3)");
  EXPECT_EQ(sx("8 / 4 / 2"), "(/ (/ 8 4) 2)");
}

TEST(Expr, MulBindsTighter) { EXPECT_EQ(sx("1 + 2 * 3"), "(+ 1 (* 2 3))"); }

TEST(Expr, UnaryMinusTightest) {
  EXPECT_EQ(sx("-a * b"), "(* (neg a) b)");
  EXPECT_EQ(sx("- - a"), "(neg (neg a))");
  EXPECT_EQ(sx("a - -b"), "(- a (neg b))");
}

TEST(Expr, Parens) { EXPECT_EQ(sx("(1 + 2) * 3"), "(* (+ 1 2) 3)"); }

TEST(Expr, CallsAndIndex) {
  EXPECT_EQ(sx("f(a, g(b), c[1])"), "(call f a (call g b) (index c 1))");
  EXPECT_EQ(sx("length(a)"), "(call length a)");
  EXPECT_EQ(sx("f()"), "(call f)");
  EXPECT_EQ(sx("a[i + 1]"), "(index a (+ i 1))");
}

TEST(Expr, HugeLiteralKeptVerbatim) { EXPECT_EQ(sx("99999999999999999999"), "99999999999999999999"); }

TEST(Expr, TreeFormat) {
  auto r = parse_expression("a[i] + 1");
  ASSERT_TRUE(r.node);
  EXPECT_EQ(print_tree(*r.node), "Binary + @1:1\n  ArrayAccess a @1:1\n    Var i @1:3\n  IntLit 1 @1:8\n");
}

TEST(Expr, Errors) {
  EXPECT_EQ(err("1 +"), "t:1:4: ошибка: ожидается выражение, найден конец файла");
  EXPECT_EQ(err("(1 + 2"), "t:1:7: ошибка: ожидается ')', найден конец файла");
  EXPECT_EQ(err("1 2"), "t:1:3: ошибка: ожидается конец файла, найдено '2'");
  EXPECT_EQ(err("f(a b)"), "t:1:5: ошибка: ожидается ',' или ')', найдено 'b'");
  EXPECT_EQ(err("x = @"), "t:1:3: ошибка: ожидается конец файла, найдено '='\nt:1:5: ошибка: недопустимый символ '@'");
}

TEST(Expr, ErrorAtBadCharIsSuppressed) {
  EXPECT_EQ(err("1 + @"), "t:1:5: ошибка: недопустимый символ '@'");
}

TEST(Expr, DepthLimit) {
  auto r = parse_expression(std::string(5000, '(') + "1" + std::string(5000, ')'));
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "слишком глубокая вложенность (больше 1000 уровней)");
}

namespace {

std::string repeat(std::string_view s, int n) {
  std::string out;
  for (int i = 0; i < n; ++i) {
    out += s;
  }
  return out;
}

}  // namespace

TEST(Expr, LongFlatChainIsDepthLimitedNotCrash) {
  auto r = parse_expression("1" + repeat(" + 1", 20000));
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "слишком глубокая вложенность (больше 1000 уровней)");
}

TEST(Cond, LongChainsAreDepthLimitedNotCrash) {
  for (const char* op : {" -> ", " and ", " or "}) {
    auto r = parse_condition("x > 0" + repeat(std::string(op) + "x > 0", 20000));
    ASSERT_EQ(r.diagnostics.size(), 1u) << op;
    EXPECT_EQ(r.diagnostics[0].message, "слишком глубокая вложенность (больше 1000 уровней)") << op;
  }
  auto p = parse_predicate("true" + repeat(" and true", 20000));
  ASSERT_EQ(p.diagnostics.size(), 1u);
}
