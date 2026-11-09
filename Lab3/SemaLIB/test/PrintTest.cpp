#include <gtest/gtest.h>

#include "AstPrinter.h"
#include "Parser.h"
#include "Sema.h"

using namespace funny;

namespace {

constexpr const char* kSrc = "f(a: int[]) returns r: int uses t { t = a; r = t[0]; }";

// JSON без пробелов и переводов строк: проверки не зависят от отступов.
std::string compact(const std::string& json) {
  std::string out;
  for (char c : json) {
    if (c != ' ' && c != '\n') {
      out += c;
    }
  }
  return out;
}

}  // namespace

TEST(Print, TreeBeforeCheckHasNoAnnotations) {
  ParseResult p = parse(kSrc);
  ASSERT_TRUE(p.ok());
  const std::string tree = print_tree(p.module);
  EXPECT_EQ(tree.find(" : "), std::string::npos) << tree;
  EXPECT_EQ(tree.find('['), tree.find("a: int[]") + 6) << tree;  // единственная '[' — в типе параметра
}

TEST(Print, AnnotatedTree) {
  AnalyzeResult r = analyze(kSrc);
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(print_tree(r.module),
            "Module\n"
            "  Function f @1:1\n"
            "    params\n      a: int[] @1:3\n"
            "    returns\n      r: int @1:21\n"
            "    uses\n      t: int[] (inferred) @1:33\n"
            "    body\n"
            "      Block @1:35\n"
            "        Assign t @1:37\n          Var a [param] : int[] @1:41\n"
            "        Assign r @1:44\n          ArrayAccess t : int @1:48\n            IntLit 0 : int @1:50\n");
}

TEST(Print, AnnotatedJson) {
  AnalyzeResult r = analyze(kSrc);
  ASSERT_TRUE(r.ok());
  const std::string j = compact(print_json(r.module));
  EXPECT_NE(
      j.find(R"("kind":"LocalDef","pos":{"line":1,"col":33,"offset":32},"name":"t","type":"int[]","inferred":true)"),
      std::string::npos)
      << j;
  EXPECT_NE(j.find(R"("kind":"Var","pos":{"line":1,"col":41,"offset":40},"name":"a","symbol":"param","type":"int[]")"),
            std::string::npos)
      << j;
  EXPECT_NE(j.find(R"("value":"0","type":"int"},"type":"int"},"sugar":null)"), std::string::npos) << j;
}

TEST(Print, TupleTypeInJson) {
  AnalyzeResult r = analyze(
      "d(a: int) returns q: int, s: int[] { q = a; s = s2(); }\ns2() returns s: int[] s = s2();\n"
      "f() returns x: int, y: int[] x, y = d(1);");
  ASSERT_TRUE(r.ok());
  EXPECT_NE(compact(print_json(r.module)).find(R"("callee":"d","args":[{"kind":"IntLit")"), std::string::npos);
  EXPECT_NE(compact(print_json(r.module)).find(R"("type":["int","int[]"])"), std::string::npos);
  EXPECT_NE(print_tree(r.module).find("Call d : (int, int[]) @3:37"), std::string::npos) << print_tree(r.module);
}

TEST(Print, UnannotatedJsonFieldsAreNull) {
  ParseResult p = parse("f() returns r: int uses t r = t;");
  ASSERT_TRUE(p.ok());
  const std::string j = compact(print_json(p.module));
  EXPECT_NE(j.find(R"("name":"t","symbol":null,"type":null)"), std::string::npos) << j;
  EXPECT_NE(j.find(R"("name":"t","type":null,"inferred":false)"), std::string::npos) << j;
}
