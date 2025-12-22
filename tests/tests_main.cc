// tests/tests_main.cc
#include <cstdio>

#include "framework.hh"

int main() {
  int total = 0;
  int failed_cases = 0;
  for (auto& c : slate::test::registry()) {
    int before = slate::test::failure_count();
    c.fn();
    int after = slate::test::failure_count();
    ++total;
    bool ok = (after == before);
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", c.name.c_str());
    if (!ok) ++failed_cases;
  }
  std::printf("\n%d/%d cases passed (%d assertion failures)\n",
              total - failed_cases, total, slate::test::failure_count());
  return failed_cases == 0 ? 0 : 1;
}
