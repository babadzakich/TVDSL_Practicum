#include <gtest/gtest.h>

#include "SemaSnapshot.h"

TEST(Snapshots, AllCasesPass) {
  auto results = funny::run_sema_snapshots(FUNNY_TESTS_DIR, /*bless=*/false);
  ASSERT_GE(results.size(), 40u);
  for (const auto& r : results) {
    EXPECT_TRUE(r.passed) << r.source << " (" << r.kind << ")\nexpected:\n" << r.expected << "\nactual:\n" << r.actual;
  }
}
