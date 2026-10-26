#include <gtest/gtest.h>

#include <string>

#include "AstPrinter.h"
#include "Parser.h"
#include "SExpr.h"

using namespace funny;

namespace {

std::string to_sx(const FragmentResult<CondPtr>& r, std::string_view src) {
  EXPECT_TRUE(r.diagnostics.empty()) << src << ": " << (r.diagnostics.empty() ? "" : r.diagnostics[0].message);
  return r.node ? sexpr(*r.node) : "<null>";
}

std::string to_err(const FragmentResult<CondPtr>& r) {
  std::string out;
  for (const Diagnostic& d : r.diagnostics) {
    out += (out.empty() ? "" : "\n") + format_diagnostic("t", d);
  }
  return out;
}

std::string csx(std::string_view src) { return to_sx(parse_condition(src), src); }
std::string psx(std::string_view src) { return to_sx(parse_predicate(src), src); }
std::string cerr(std::string_view src) { return to_err(parse_condition(src)); }
std::string perr(std::string_view src) { return to_err(parse_predicate(src)); }

}  // namespace

TEST(Cond, Precedence) {
  EXPECT_EQ(csx("a > 0 and b > 0 or c > 0"), "(or (and (> a 0) (> b 0)) (> c 0))");
  EXPECT_EQ(csx("not a == b and c != d"), "(and (not (== a b)) (!= c d))");
  EXPECT_EQ(csx("true or false"), "(or true false)");
}

TEST(Cond, ImplicationRightAssoc) { EXPECT_EQ(csx("a > 0 -> b > 0 -> c > 0"), "(-> (> a 0) (-> (> b 0) (> c 0)))"); }

TEST(Cond, ParenAmbiguity) {
  EXPECT_EQ(csx("(a + b) > c"), "(> (+ a b) c)");
  EXPECT_EQ(csx("(a > b)"), "(> a b)");
  EXPECT_EQ(csx("((a > b) and (c > d))"), "(and (> a b) (> c d))");
}

TEST(Cond, DeepParensFinish) {
  EXPECT_EQ(csx(std::string(300, '(') + "x > 0" + std::string(300, ')')), "(> x 0)");
}

TEST(Pred, QuantifiersAndRefs) {
  EXPECT_EQ(psx("forall(i: int | a[i] >= 0)"), "(forall i:int (>= (index a i) 0))");
  EXPECT_EQ(psx("exists(b: int[] | p(b))"), "(exists b:int[] (ref p b))");
  EXPECT_EQ(psx("sorted(a, n) and f(x) == 3"), "(and (ref sorted a n) (== (call f x) 3))");
}

TEST(Cond, Errors) {
  EXPECT_EQ(cerr("x = 1"), "t:1:3: ошибка: ожидается '==', '!=', '>=', '<=', '>' или '<', найдено '='");
  EXPECT_EQ(cerr("forall(i: int | i > 0)"), "t:1:1: ошибка: ожидается условие, найдено 'forall'");
  EXPECT_EQ(cerr("a < b < c"), "t:1:7: ошибка: ожидается конец файла, найдено '<'");
  EXPECT_EQ(perr("a > 0 -> b > 0"), "t:1:7: ошибка: ожидается конец файла, найдено '->'");
  EXPECT_EQ(perr("forall(i int | true)"), "t:1:10: ошибка: ожидается ':', найдено 'int'");
}

TEST(Cond, TreeFormat) {
  auto r = parse_predicate("not forall(i: int | i > 0)");
  ASSERT_TRUE(r.node);
  EXPECT_EQ(print_tree(*r.node),
            "Not @1:1\n  Quantifier forall i: int @1:5\n    Compare > @1:21\n      Var i @1:21\n      IntLit 0 @1:25\n");
}
