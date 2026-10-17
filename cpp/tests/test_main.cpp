#include "check.h"
int main() {
  for (auto& t : registry()) {
    int before = failures();
    t.fn();
    std::printf("[%s] %s\n", failures() == before ? "ok" : "FAIL", t.name);
  }
  std::printf("%d failure(s)\n", failures());
  return failures() ? 1 : 0;
}