#include <gtest/gtest.h>

#include "TestUtil.h"

TEST(Init, ParamsAreAlwaysInitialized) {
  EXPECT_EQ(errors("f(x: int) returns r: int r = x;"), Errors{});
}

TEST(Init, ReturnsAndLocalsMustBeAssignedBeforeRead) {
  EXPECT_EQ(errors("f(x: int) returns r: int uses t: int { r = r + t; r = t; }"),
            (Errors{"1:44: переменная 'r' используется до присваивания",
                    "1:48: переменная 't' используется до присваивания"}));
}

TEST(Init, IfNeedsBothBranches) {
  EXPECT_EQ(errors("f(x: int) returns r: int uses t: int { if (x > 0) t = 1; else t = 2; r = t; }"), Errors{});
  EXPECT_EQ(errors("f(x: int) returns r: int uses t: int { if (x > 0) t = 1; else { } r = t; }"),
            (Errors{"1:71: переменная 't' используется до присваивания"}));
  EXPECT_EQ(errors("f(x: int) returns r: int uses t: int { if (x > 0) t = 1; r = t; }"),
            (Errors{"1:62: переменная 't' используется до присваивания"}));
}

TEST(Init, LoopBodyMayNotRun) {
  EXPECT_EQ(errors("f(x: int) returns r: int { while (x > 0) r = 1; r = r + 1; }"),
            (Errors{"1:53: переменная 'r' используется до присваивания"}));
  EXPECT_EQ(errors("f(x: int) returns r: int { r = 0; while (r < x) invariant r <= x r = r + 1; }"), Errors{});
}

TEST(Init, InvariantAndAssertAreCheckedInPlace) {
  EXPECT_EQ(
      errors("f(x: int) returns r: int uses i: int { assert i > 0; while (x > 0) invariant r > 0 x = 1; r = 0; }"),
      (Errors{"1:47: переменная 'i' используется до присваивания", "1:78: переменная 'r' используется до присваивания",
              "1:84: параметр 'x' доступен только для чтения"}));
}

TEST(Init, EnsuresIsNotCheckedForInitialization) {
  EXPECT_EQ(errors("f(x: int) returns r: int ensures r == x { r = x; }"), Errors{});
}

TEST(Init, TupleAssignInitializesAllTargets) {
  EXPECT_EQ(errors("d(a: int) returns q: int, r: int { q = a; r = a; }\n"
                   "f(x: int) returns s: int uses q, r { q, r = d(x); s = q + r; }"),
            Errors{});
}
