#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "AstPrinter.h"
#include "Parser.h"

using namespace funny;

TEST(Json, Expression) {
  auto r = parse_expression("-x");
  ASSERT_TRUE(r.node);
  EXPECT_EQ(print_json(*r.node),
            "{\n  \"kind\": \"Neg\",\n  \"pos\": {\"line\": 1, \"col\": 1, \"offset\": 0},\n  \"operand\": {\n"
            "    \"kind\": \"Var\",\n    \"pos\": {\"line\": 1, \"col\": 2, \"offset\": 1},\n    \"name\": \"x\"\n  }\n}\n");
}

TEST(Json, ModuleShape) {
  auto r = parse("f() returns r: int r = 1;");
  ASSERT_TRUE(r.ok());
  const std::string j = print_json(r.module);
  EXPECT_EQ(j.rfind("{\n  \"kind\": \"Module\",\n  \"decls\": [\n", 0), 0u) << j;
  EXPECT_NE(j.find("\"kind\": \"Function\""), std::string::npos);
  EXPECT_NE(j.find("\"requires\": null"), std::string::npos);
  EXPECT_NE(j.find("\"params\": []"), std::string::npos);
  EXPECT_NE(j.find("\"sugar\": null"), std::string::npos);
  EXPECT_NE(j.find("\"value\": \"1\""), std::string::npos);
}

TEST(Json, SugarMarked) {
  auto r = parse("f(v: int) returns a: int[] a[0] = v;");
  ASSERT_TRUE(r.ok());
  EXPECT_NE(print_json(r.module).find("\"sugar\": \"array_update\""), std::string::npos);
}

TEST(Json, LogicalKindsAndQuantifier) {
  auto r = parse("p(a: int[]) => not exists(i: int | a[i] > 0) or true");
  ASSERT_TRUE(r.ok());
  const std::string j = print_json(r.module);
  EXPECT_NE(j.find("\"kind\": \"Formula\""), std::string::npos);
  EXPECT_NE(j.find("\"kind\": \"Or\""), std::string::npos);
  EXPECT_NE(j.find("\"kind\": \"Not\""), std::string::npos);
  EXPECT_NE(j.find("\"quantifier\": \"exists\""), std::string::npos);
  EXPECT_NE(j.find("\"type\": \"int[]\""), std::string::npos);
}

TEST(Json, DeepTreeIsFast) {
  const std::string src = "f() returns r: int " + std::string(900, '{') + std::string(900, '}');
  auto r = parse(src);
  ASSERT_TRUE(r.ok());
  const auto start = std::chrono::steady_clock::now();
  const std::string j = print_json(r.module);
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
  EXPECT_FALSE(j.empty());
}
