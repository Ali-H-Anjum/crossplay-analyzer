#include "check.h"
#include "Board.h"
#include "Tray.h"
#include "MoveGen.h"

TEST(smoke_board_empty) {
    Board b{};
    CHECK(b.row_occ[7] == 0);
    CHECK(sq(7, 7) == 112);
}
TEST(smoke_tray_empty) {
    Tray t{};
    CHECK(t.size() == 0);
}