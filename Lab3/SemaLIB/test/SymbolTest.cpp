#include <gtest/gtest.h>

#include "AstPrinter.h"
#include "Parser.h"
#include "Sema.h"
#include "Symbols.h"
#include "TestUtil.h"

using namespace funny;

TEST(Globals, BuiltinsFunctionsAndFormulas) {
  ParseResult p = parse("p(x: int, a: int[]) => x > 0\nf(a: int[]) returns q: int, r: int[] { q = 0; r = a; }");
  ASSERT_TRUE(p.ok());
  std::vector<Diagnostic> diags;
  const GlobalScope g(p.module, diags);
  EXPECT_TRUE(diags.empty());
  ASSERT_EQ(g.symbols().size(), 3u);
  EXPECT_EQ(to_string(*g.find("length")), "builtin (int[]) -> int");
  EXPECT_EQ(to_string(*g.find("p")), "formula (int, int[]) -> bool");
  EXPECT_EQ(to_string(*g.find("f")), "function (int[]) -> (int, int[])");
  EXPECT_EQ(g.find("set"), nullptr);  // set — только для сахара a[i] = v
  EXPECT_EQ(g.find("g"), nullptr);
}

TEST(Globals, DuplicateKeepsFirst) {
  EXPECT_EQ(errors("f(x: int) returns r: int r = x;\n"
                   "f(a: int[]) returns r: int r = 0;\n"
                   "g(a: int[]) returns r: int r = f(1);"),
            (Errors{"2:1: повторное объявление 'f': уже объявлено в 1:1"}));
}

TEST(Globals, ForwardAndRecursiveCalls) {
  EXPECT_EQ(errors("a(x: int) returns r: int r = b(x);\nb(x: int) returns r: int r = a(x) + b(x);"), Errors{});
}

TEST(Locals, DuplicatesAcrossParamsReturnsLocals) {
  EXPECT_EQ(errors("f(x: int, y: int) returns y: int uses x, z: int, z { }"),
            (Errors{"1:27: повторное объявление 'y': уже объявлено в 1:11",
                    "1:39: повторное объявление 'x': уже объявлено в 1:3",
                    "1:50: повторное объявление 'z': уже объявлено в 1:42"}));
}

TEST(Locals, VariablesAndFunctionsHaveSeparateNamespaces) {
  EXPECT_EQ(errors("x(x: int) returns r: int r = x(x);"), Errors{});
}

TEST(Scopes, RequiresSeesOnlyParams) {
  EXPECT_EQ(errors("f(x: int) requires x > 0 and r > 0 and t > 0 returns r: int uses t: int { r = x; }"),
            (Errors{"1:30: 'r' недоступно в requires: там видны только параметры",
                    "1:40: 't' недоступно в requires: там видны только параметры"}));
}

TEST(Scopes, EnsuresSeesParamsAndReturns) {
  EXPECT_EQ(errors("f(x: int) returns r: int ensures r > x and t > 0 uses t: int { t = 1; r = t; }"),
            (Errors{"1:44: 't' недоступно в ensures: там видны только параметры и возвращаемые значения"}));
}

TEST(Scopes, QuantifierVariableIsLocalToItsBody) {
  EXPECT_EQ(errors("f(a: int[]) returns r: int { r = 0; assert (forall (i: int | a[i] > 0)) and i > 0; }"),
            (Errors{"1:77: неизвестное имя 'i'"}));
  // Подряд два квантора с одним именем — можно; вложенные — нельзя.
  EXPECT_EQ(errors("p(a: int[]) => (forall (i: int | a[i] > 0)) and (exists (i: int | a[i] > 1))"), Errors{});
  EXPECT_EQ(errors("p(a: int[]) => forall (i: int | exists (i: int | a[i] > 1))"),
            (Errors{"1:41: повторное объявление 'i': уже объявлено в 1:24"}));
}

TEST(Scopes, QuantifierMayReuseNameInvisibleInClause) {
  // В requires возвраты не видны, поэтому r там свободно.
  EXPECT_EQ(errors("f(a: int[]) requires forall (r: int | a[r] > 0) returns r: int r = 0;"), Errors{});
}

TEST(Scopes, FormulaSeesOnlyItsParams) {
  EXPECT_EQ(errors("f(x: int) returns r: int r = x;\np(y: int) => y > x"), (Errors{"2:18: неизвестное имя 'x'"}));
}

TEST(Symbols, VarKindsAreAnnotated) {
  AnalyzeResult r =
      analyze("f(a: int[]) returns s: int uses i { i = 0; s = a[i] + i; assert forall (k: int | k > s); }");
  ASSERT_TRUE(r.ok());
  const std::string tree = print_tree(r.module);
  EXPECT_NE(tree.find("Var i [local] : int"), std::string::npos) << tree;
  EXPECT_NE(tree.find("Var s [return] : int"), std::string::npos) << tree;
  EXPECT_NE(tree.find("Var k [bound] : int"), std::string::npos) << tree;
}

TEST(Symbols, PrintSymbols) {
  AnalyzeResult r = analyze("p(x: int) => x > 0\nf(a: int[]) returns s: int uses i, j: int { i = 1; j = i; s = j; }");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(print_symbols(r.module),
            "globals\n"
            "  length: builtin (int[]) -> int\n"
            "  p: formula (int) -> bool @1:1\n"
            "  f: function (int[]) -> int @2:1\n"
            "formula p @1:1\n"
            "  x: int [param] @1:3\n"
            "function f @2:1\n"
            "  a: int[] [param] @2:3\n"
            "  s: int [return] @2:21\n"
            "  i: int [local, inferred] @2:33\n"
            "  j: int [local] @2:36\n");
}
