#include "check.h"
#include "MoveEval.h"

TEST(premium_array_matches_map) {
    for (int r = 0; r < BOARD_LENGTH; ++r)
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            auto it = premiumMap().find(sq(r, c));
            Mult want = it == premiumMap().end() ? MULT_NONE : it->second;
            CHECK(premiumAt(r, c) == want);
        }
}
TEST(premium_spot_checks) {
    CHECK(premiumAt(0, 0) == MULT_TL);
    CHECK(premiumAt(0, 3) == MULT_TW);
    CHECK(premiumAt(0, 7) == MULT_DL);
    CHECK(premiumAt(1, 1) == MULT_DW);
    CHECK(premiumAt(7, 7) == MULT_NONE);
}