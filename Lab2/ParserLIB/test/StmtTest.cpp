#include <gtest/gtest.h>

#include <string>

#include "AstPrinter.h"
#include "Parser.h"
#include "SExpr.h"

using namespace funny;

namespace {

std::string msx(std::string_view src) {
  auto r = parse(src);
  EXPECT_TRUE(r.ok()) << src << ": " << (r.diagnostics.empty() ? "" : format_diagnostic("t", r.diagnostics[0]));
  std::string out;
  for (const Decl& d : r.module.decls) {
    out += (out.empty() ? "" : "\n") + sexpr(d);
  }
  return out;
}

}  // namespace

TEST(Module, FunctionWithoutKeyword) {
  EXPECT_EQ(msx("f(x: int) returns y: int y = x;"), "(function f (params x:int) (returns y:int) (= y x))");
}

TEST(Module, FormulaAndFullHeader) {
  EXPECT_EQ(msx("p(x: int) => x > 0\n"
                "g(a: int[]) requires p(1) returns r: int, s: int[] ensures true uses i, t: int { }"),
            "(formula p (params x:int) (> x 0))\n"
            "(function g (params a:int[]) (requires (ref p 1)) (returns r:int s:int[]) (ensures true) (uses i t:int) "
            "(block))");
}

TEST(Stmt, ArraySugarAndTuple) {
  EXPECT_EQ(msx("f(v: int) returns a: int[] { a[0] = v; }"),
            "(function f (params v:int) (returns a:int[]) (block (=sugar a (call set a 0 v))))");
  EXPECT_EQ(msx("f(a: int, b: int) returns q: int, r: int q, r = divmod(a, b);"),
            "(function f (params a:int b:int) (returns q:int r:int) (= (q r) (call divmod a b)))");
  EXPECT_EQ(msx("f(y: int) returns x: int x = g(y);"), "(function f (params y:int) (returns x:int) (= x (call g y)))");
}

TEST(Stmt, DanglingElseBindsInner) {
  EXPECT_EQ(msx("f(a: int, b: int) returns x: int if (a > 0) if (b > 0) x = 1; else x = 2;"),
            "(function f (params a:int b:int) (returns x:int) (if (> a 0) (if (> b 0) (= x 1) (= x 2))))");
}

TEST(Stmt, WhileAssertAssume) {
  EXPECT_EQ(msx("f(n: int) returns i: int { i = 0; while (i < n) invariant i <= n { i = i + 1; } assert i == n; "
                "assume true; }"),
            "(function f (params n:int) (returns i:int) (block (= i 0) (while (< i n) (inv (<= i n)) (block (= i (+ i "
            "1)))) (assert (== i n)) (assume true)))");
}

TEST(Module, TreeFormat) {
  auto r = parse("f(a: int[]) returns r: int\n  r = a[0];");
  EXPECT_EQ(print_tree(r.module),
            "Module\n  Function f @1:1\n    params\n      a: int[] @1:3\n    returns\n      r: int @1:21\n"
            "    body\n      Assign r @2:3\n        ArrayAccess a @2:7\n          IntLit 0 @2:9\n");
}

TEST(Module, TreeFormatStatements) {
  auto r = parse("f(v: int) returns a: int[] uses i { if (v > 0) a[i] = v; else { } while (true) invariant true i = 1; }");
  EXPECT_EQ(print_tree(r.module),
            "Module\n"
            "  Function f @1:1\n"
            "    params\n      v: int @1:3\n"
            "    returns\n      a: int[] @1:19\n"
            "    uses\n      i @1:33\n"
            "    body\n"
            "      Block @1:35\n"
            "        If @1:37\n"
            "          cond\n            Compare > @1:41\n              Var v @1:41\n              IntLit 0 @1:45\n"
            "          then\n"
            "            Assign a [sugar: array_update] @1:48\n"
            "              Call set @1:48\n                Var a @1:48\n                Var i @1:50\n                Var v @1:55\n"
            "          else\n            Block @1:63\n"
            "        While @1:67\n"
            "          cond\n            BoolLit true @1:74\n"
            "          invariant\n            BoolLit true @1:90\n"
            "          body\n            Assign i @1:95\n              IntLit 1 @1:99\n");
}

TEST(Module, EmptyInputIsError) {
  auto r = parse("");
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]),
            "t:1:1: ошибка: ожидается объявление функции или формулы, найден конец файла");
}

TEST(Stmt, DeepStatementNesting) {
  std::string src = "f() returns r: int " + std::string(5000, '{') + std::string(5000, '}');
  auto r = parse(src);
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "слишком глубокая вложенность (больше 1000 уровней)");
}
