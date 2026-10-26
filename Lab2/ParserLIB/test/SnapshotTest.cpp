#include <gtest/gtest.h>

#include "Snapshot.h"

TEST(Snapshots, AllCasesPass) {
  auto results = funny::run_snapshots(FUNNY_TESTS_DIR, /*bless=*/false);
  ASSERT_GE(results.size(), 10u);
  for (const auto& r : results) {
    EXPECT_TRUE(r.passed) << r.source << " (" << r.kind << ")\nexpected:\n" << r.expected << "\nactual:\n" << r.actual;
  }
}

TEST(Snapshots, RenderDiagnostics) {
  EXPECT_EQ(funny::render_diagnostics("a.funny", {{funny::Pos{2, 3, 7}, "m"}}), "a.funny:2:3: ошибка: m\n");
}
