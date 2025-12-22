// tests/framework.hh
//
// A dependency-free test registry. Tests register themselves via the TEST macro
// and the runner in tests_main.cc executes them, counting failures. No external
// framework so the test build stays offline and trivial.
#ifndef SLATE_TESTS_FRAMEWORK_HH
#define SLATE_TESTS_FRAMEWORK_HH

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace slate::test {

struct Case {
  std::string name;
  std::function<void()> fn;
};

// Global failure counter for the currently running case.
inline int& failure_count() {
  static int n = 0;
  return n;
}

inline std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

struct Register {
  Register(const std::string& name, std::function<void()> fn) {
    registry().push_back(Case{name, std::move(fn)});
  }
};

}  // namespace slate::test

#define SLATE_CAT2(a, b) a##b
#define SLATE_CAT(a, b) SLATE_CAT2(a, b)

#define TEST(name)                                                          \
  static void SLATE_CAT(slate_test_, __LINE__)();                           \
  static ::slate::test::Register SLATE_CAT(slate_reg_, __LINE__)(           \
      name, &SLATE_CAT(slate_test_, __LINE__));                             \
  static void SLATE_CAT(slate_test_, __LINE__)()

#define CHECK(cond)                                                         \
  do {                                                                      \
    if (!(cond)) {                                                          \
      ++::slate::test::failure_count();                                     \
      std::printf("    CHECK failed: %s  (%s:%d)\n", #cond, __FILE__,       \
                  __LINE__);                                                \
    }                                                                       \
  } while (0)

#define CHECK_EQ(a, b)                                                      \
  do {                                                                      \
    auto va = (a);                                                          \
    auto vb = (b);                                                          \
    if (!(va == vb)) {                                                      \
      ++::slate::test::failure_count();                                     \
      std::printf("    CHECK_EQ failed: %s == %s  (%s:%d)\n", #a, #b,       \
                  __FILE__, __LINE__);                                      \
    }                                                                       \
  } while (0)

#endif  // SLATE_TESTS_FRAMEWORK_HH
