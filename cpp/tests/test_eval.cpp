#include "check.h"
#include "eval.h"
#include "test_common.h"

static void setup(Position& p, Board& b, TileBag& bag, const Gaddag& g) {
    p.b = &b;
    p.g = &g;
    p.init = &g.get_init();
    p.bag = &bag;
    rebuild(b, p.al, g, g.get_init());
    p.tray[0].addTiles("AEINRST");
    p.tray[1].addTiles("QVJKXZU");
    p.score[0] = 50;
    p.score[1] = 20;
    p.side = 1;
}

TEST(eval_antisymmetric) {
    const Gaddag& g = testGaddag();
    Board b{};
    TileBag bag(1);
    Position p;
    setup(p, b, bag, g);
    CHECK(evaluate(p, 0) == -evaluate(p, 1));
    CHECK(evaluate(p, 0) > 0);  // me is ahead on score and leave
}
TEST(eval_endgame_is_score_only) {
    const Gaddag& g = testGaddag();
    Board b{};
    TileBag bag(1);
    Position p;
    setup(p, b, bag, g);
    std::array<uint8_t, NUM_LETTER_SLOTS> none{};
    bag.setPool(none);
    CHECK(bag.empty());
    CHECK(evaluate(p, 0) == 10 * 30);
}
TEST(eval_big_premium_table) {
    int n = 0;
    for (uint8_t i : kBigPremium) {
        CHECK(kPremium[i] == MULT_DW || kPremium[i] == MULT_TW);
        n += (i != 0);
    }
    CHECK(n == 16);
    Board b{};
    AnchorLines al;
    rebuild(b, al, testGaddag(), testGaddag().get_init());
    CHECK(openBigPremiums(al) == 0);  // empty board: only the centre anchor
}