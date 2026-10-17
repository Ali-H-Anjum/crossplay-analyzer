#include "check.h"
#include "tile_bag.h"

static bool trayEq(const Tray& a, const Tray& b) {
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        if (a.count(fromIndex(static_cast<uint8_t>(i))) != b.count(fromIndex(static_cast<uint8_t>(i)))) return false;
    return a.size() == b.size();
}

TEST(bag_drawnext_undo_exact) {
    TileBag bag(123); bag.shuffle();
    const auto before = bag.tilesCopy(); const uint16_t tot = bag.totalCount();
    Tray t{};
    const uint8_t a = bag.drawNextInto(t, 7), b = bag.drawNextInto(t, 3);
    CHECK(a == 7 && b == 3 && t.size() == 10 && bag.size() == tot - 10);
    bag.undoDraw(t, b); bag.undoDraw(t, a);
    CHECK(t.size() == 0 && bag.totalCount() == tot && bag.tilesCopy() == before);
}

TEST(bag_same_seed_same_order) {
    TileBag x(42), y(42); x.shuffle(); y.shuffle();
    Tray tx{}, ty{}; x.drawNextInto(tx, 7); y.drawNextInto(ty, 7);
    CHECK(trayEq(tx, ty));
}

TEST(bag_pool_and_clamp) {
    TileBag bag(7);
    std::array<uint8_t, NUM_LETTER_SLOTS> pool{}; pool[0] = 3; pool[1] = 2;   // 3xA, 2xB
    bag.setPool(pool); bag.shuffle();
    CHECK(bag.size() == 5);
    Tray t{};
    const uint8_t n = bag.drawNextInto(t, 7);
    CHECK(n == 5 && t.size() == 5 && bag.empty());
    CHECK(t.count(fromIndex(0)) == 3 && t.count(fromIndex(1)) == 2);
    bag.undoDraw(t, n);
    CHECK(t.size() == 0 && bag.size() == 5);
}