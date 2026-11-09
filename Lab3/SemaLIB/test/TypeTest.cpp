#include <gtest/gtest.h>

#include "AstPrinter.h"
#include "Parser.h"
#include "Sema.h"
#include "TestUtil.h"

using namespace funny;

TEST(Types, EveryExpressionGetsAType) {
  ParseResult p = parse(
      "g(a: int[]) returns q: int, r: int[] { q = 0; r = a; }\n"
      "f(a: int[], i: int) returns s: int uses q, r { q, r = g(a); s = -a[i] * length(r) + q / 2; }");
  ASSERT_TRUE(p.ok());
  ASSERT_TRUE(check(p.module).empty());
  const auto& f = std::get<Function>(p.module.decls[1]);
  const auto& body = std::get<Block>(f.body->node).body;
  const auto& tuple = std::get<TupleAssign>(body[0]->node);
  EXPECT_EQ(tuple.call->type, (ExprType{TypeKind::Int, TypeKind::IntArray}));
  const Expr& value = *std::get<Assign>(body[1]->node).value;
  EXPECT_EQ(value.type, ExprType{TypeKind::Int});
  // Обходим всё дерево: ни одного выражения без типа.
  const std::string tree = print_tree(p.module);
  for (const char* kind : {"IntLit", "Var ", "ArrayAccess", "Call", "Neg", "Binary"}) {
    for (std::size_t at = tree.find(kind); at != std::string::npos; at = tree.find(kind, at + 1)) {
      const std::size_t eol = tree.find('\n', at);
      EXPECT_NE(tree.substr(at, eol - at).find(" : "), std::string::npos) << tree.substr(at, eol - at);
    }
  }
}

TEST(Types, ArithmeticAndComparisonNeedInt) {
  EXPECT_EQ(
      errors("f(a: int[], x: int) returns r: int { r = a - x; if (a > x) r = 0; while (x != a) r = 1; }"),
      (Errors{"1:42: операнд '-': ожидается int, получено int[]", "1:53: сравнение '>': ожидается int, получено int[]",
              "1:79: сравнение '!=': ожидается int, получено int[]"}));
}

TEST(Types, ArraysAndIndexes) {
  EXPECT_EQ(errors("f(a: int[], x: int) returns r: int, b: int[] { r = x[0]; r = a[a]; b = a; b[x] = a; }"),
            (Errors{"1:52: обращение по индексу к 'x': ожидается int[], получено int",
                    "1:64: индекс массива: ожидается int, получено int[]",
                    "1:82: значение элемента массива: ожидается int, получено int[]"}));
}

TEST(Types, ArrayUpdateOnScalar) {
  EXPECT_EQ(errors("f(x: int) returns r: int { r = x; r[0] = 1; }"),
            (Errors{"1:35: обновление элемента 'r': ожидается int[], получено int"}));
}

TEST(Types, CallsCheckArityAndArgumentTypes) {
  EXPECT_EQ(
      errors("g(x: int, a: int[]) returns r: int r = x;\n"
             "f(a: int[]) returns r: int { r = g(1); r = g(a, a, 1); r = length(1); }"),
      (Errors{"2:34: функция 'g' ожидает 2 аргумента, передано 1", "2:44: функция 'g' ожидает 2 аргумента, передано 3",
              "2:46: аргумент 1 функции 'g': ожидается int, получено int[]",
              "2:67: аргумент 1 функции 'length': ожидается int[], получено int"}));
}

TEST(Types, FormulaRefsCheckArguments) {
  EXPECT_EQ(errors("p(a: int[]) => length(a) > 0\nf(x: int) requires p(x) and p() returns r: int r = x;"),
            (Errors{"2:22: аргумент 1 формулы 'p': ожидается int[], получено int",
                    "2:29: формула 'p' ожидает 1 аргумент, передано 0"}));
}

TEST(Types, MultipleResults) {
  const char* divmod = "d(a: int, b: int) returns q: int, r: int { q = a / b; r = a - q * b; }\n";
  EXPECT_EQ(errors(std::string(divmod) + "f(x: int) returns r: int r = d(x, 1) * 2;"),
            (Errors{"2:30: функция 'd' возвращает 2 значения; в выражении допустим только вызов с одним результатом"}));
  EXPECT_EQ(errors(std::string(divmod) + "f(x: int) returns a: int, b: int[] a, b = d(x, 1);"),
            (Errors{"2:39: присваивание 'b': переменная имеет тип int[], значение — int"}));
  EXPECT_EQ(errors(std::string(divmod) + "f(x: int) returns a: int, b: int, c: int a, b, c = d(x, 1);"),
            (Errors{"2:52: функция 'd' возвращает 2 значения, а в левой части 3 переменные"}));
}

TEST(Types, ErrorsDoNotCascade) {
  // Неизвестное имя даёт ошибочный тип, совместимый со всем: дальше по выражению ошибок нет.
  EXPECT_EQ(errors("f(a: int[]) returns r: int r = (y + 1) * length(y) + g(y);"),
            (Errors{"1:33: неизвестное имя 'y'", "1:49: неизвестное имя 'y'", "1:54: неизвестная функция 'g'",
                    "1:56: неизвестное имя 'y'"}));
}

TEST(Types, IntLiteralRangeOnlyInCode) {
  EXPECT_EQ(errors("f() returns r: int ensures r < 4294967296 { r = -2147483648; r = 2147483648; }"),
            (Errors{"1:66: константа 2147483648 не помещается в int (32 бита)"}));
}
