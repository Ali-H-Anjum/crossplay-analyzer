#include "check.h"
#include "move_eval.h"

TEST(premium_array_matches_map) {
    // kPremium is the single source of truth now (constexpr array).
    // Verify premiumAt() matches the direct array lookup on every square.
    for (int r = 0; r < BOARD_LENGTH; ++r)
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            CHECK(premiumAt(r, c) == kPremium[sq(r, c)]);
        }
}
TEST(premium_spot_checks) {
    CHECK(premiumAt(0, 0) == MULT_TL);
    CHECK(premiumAt(0, 3) == MULT_TW);
    CHECK(premiumAt(0, 7) == MULT_DL);
    CHECK(premiumAt(1, 1) == MULT_DW);
    CHECK(premiumAt(7, 7) == MULT_NONE);
}