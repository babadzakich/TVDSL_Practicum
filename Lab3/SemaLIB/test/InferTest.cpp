#include <gtest/gtest.h>

#include "Parser.h"
#include "Sema.h"
#include "TestUtil.h"

using namespace funny;

namespace {

// Типы локальных после анализа: "a:int b:int[] c:?"
std::string locals(std::string_view src) {
  AnalyzeResult r = analyze(src);
  std::string out;
  for (const LocalDef& l : std::get<Function>(r.module.decls.back()).uses) {
    out += (out.empty() ? "" : " ") + l.name + ':' + (l.type ? to_string(*l.type) : "?") + (l.inferred ? "*" : "");
  }
  return out;
}

}  // namespace

TEST(Infer, FromLiteralsCallsAndOtherLocals) {
  EXPECT_EQ(locals("g(n: int) returns a: int[] a = g(n);\n"
                   "f(v: int[]) returns r: int uses a, b, c, d: int, e { a = 1; b = a; c = g(b); d = 2; e = length(c); "
                   "r = 0; }"),
            "a:int* b:int* c:int[]* d:int e:int*");
}

TEST(Infer, FromTupleAssignment) {
  EXPECT_EQ(locals("g(n: int) returns a: int[], k: int { a = g2(); k = 0; }\ng2() returns a: int[] a = g2();\n"
                   "f() returns r: int uses x, y { x, y = g(1); r = y; }"),
            "x:int[]* y:int*");
}

TEST(Infer, FromArrayUpdateSugar) {
  // a[i] = 42 обессахаривается в a = set(a, i, 42) и фиксирует a: int[] (чтение a до присваивания — ошибка).
  EXPECT_EQ(locals("f(v: int[]) returns r: int uses a { a = v; a[0] = 42; r = a[0]; }"), "a:int[]*");
  EXPECT_EQ(errors("f() returns r: int uses a { a[0] = 42; r = 0; }"),
            (Errors{"1:29: переменная 'a' используется до присваивания"}));
  EXPECT_EQ(locals("f() returns r: int uses a { a[0] = 42; r = 0; }"), "a:int[]*");
}

TEST(Infer, TwoUnknownsGiveNothing) {
  // b = a, когда тип не известен ни у кого: выводить не из чего (a читается до присваивания).
  EXPECT_EQ(errors("f() returns r: int uses a, b { b = a; a = 1; r = 0; }"),
            (Errors{"1:28: не удалось вывести тип локальной переменной 'b': укажите тип явно",
                    "1:36: переменная 'a' используется до присваивания"}));
  // Тип b можно вывести позже из другого присваивания: r = b, r: int.
  EXPECT_EQ(locals("f() returns r: int uses a, b { b = a; a = 1; r = b; }"), "a:int* b:int*");
}

TEST(Infer, ConflictMentionsWhereTypeWasInferred) {
  EXPECT_EQ(errors("f(v: int[]) returns r: int uses t { t = v; r = t * 2; t = 3; }"),
            (Errors{"1:48: операнд '*': ожидается int, получено int[] (тип 't' выведен в 1:41)",
                    "1:59: присваивание 't': переменная имеет тип int[] (тип 't' выведен в 1:41), значение — int"}));
}

TEST(Infer, FirstCorrectAssignmentWins) {
  EXPECT_EQ(errors("f(v: int[]) returns r: int uses t { t = w; t = v; t = 1; r = 0; }"),
            (Errors{"1:41: неизвестное имя 'w'",
                    "1:55: присваивание 't': переменная имеет тип int[] (тип 't' выведен в 1:48), значение — int"}));
  EXPECT_EQ(locals("f(v: int[]) returns r: int uses t { t = w; t = v; r = 0; }"), "t:int[]*");
}

TEST(Infer, ImpossibleInferenceIsError) {
  EXPECT_EQ(errors("f() returns r: int uses never, bad { bad = nope; r = 0; }"),
            (Errors{"1:25: не удалось вывести тип локальной переменной 'never': укажите тип явно",
                    "1:32: не удалось вывести тип локальной переменной 'bad': укажите тип явно",
                    "1:44: неизвестное имя 'nope'"}));
}

TEST(Infer, ContextFromParameters) {
  EXPECT_EQ(locals("g(a: int[], i: int) returns r: int r = a[i];\n"
                   "f(v: int[]) returns r: int uses x, y { x = v; y = 0; r = g(x, y); }"),
            "x:int[]* y:int*");
}
