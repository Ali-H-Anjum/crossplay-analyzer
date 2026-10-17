#include <chrono>
#include <thread>

#include "check.h"
#include "search_job.h"
#include "test_common.h"

namespace {
struct JobFix {
  Board b{};
  TileBag bag;
  Position p;
  explicit JobFix(uint64_t seed) : bag(seed) {
    const Gaddag& g = testGaddag();
    p.b = &b;
    p.g = &g;
    p.init = &g.get_init();
    p.bag = &bag;
    rebuild(b, p.al, g, g.get_init());
    bag.shuffle();
    bag.drawNextInto(p.tray[0], 7);
    bag.drawNextInto(p.tray[1], 7);
  }
};
long long msSince(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - t)
      .count();
}
}  // namespace

TEST(job_matches_sync_and_is_isolated) {
  JobFix f(11);
  SearchConfig c;
  c.depth = 2;
  const SearchResult sync = Searcher(f.p, c).run();
  SearchResult r;
  {
    SearchJob job(f.p, c);
    f.p.score[0] = 999;
    f.b.row_occ[0] = 1;  // mutate the source while the job runs
    while (!job.finished())
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    r = job.take();
    f.p.score[0] = 0;
    f.b.row_occ[0] = 0;
  }
  CHECK(r.hasMove && r.value == sync.value && r.nodes == sync.nodes);
}

TEST(job_cancel_is_fast) {
  JobFix f(23);
  SearchConfig c;
  c.iterative = true;  // 10 minute default limit
  const auto t = std::chrono::steady_clock::now();
  SearchResult r;
  {
    SearchJob job(f.p, c);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(!job.finished());
    job.cancel();
    r = job.take();
  }
  std::printf("  cancelled after %lld ms\n", msSince(t));
  CHECK(r.hasMove && msSince(t) < 2000);
}

TEST(job_destructor_stops_search) {
  JobFix f(5);
  SearchConfig c;
  c.iterative = true;
  const auto t = std::chrono::steady_clock::now();
  {
    SearchJob job(f.p, c);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  CHECK(msSince(t) < 2000);
}