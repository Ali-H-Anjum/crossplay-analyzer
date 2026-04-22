from Board import Board
from Move import Move
from Tiles import Tiles
from MoveGenerator import MoveGenerator
from gaddagTesting import GADDAG
from MoveEvaluator import MoveEvaluator

g = GADDAG()

mg = MoveGenerator(g)

b = Board()

b.add_move(Move('ACETATE', 5, 7, False))
b.add_move(Move('WIGGERY', 7,11, True))

b.show_board()
me = MoveEvaluator(b)

t = Tiles(['D', 'P', 'C', 'Y', 'O', 'E', 'G'])

print(t)

moves = mg.get_all_moves(b, t)
points_per_move = me.sort_by_points(moves)

highest_moves = me.get_top_number(points_per_move, 15)
most_points, move_with_most_points = highest_moves[0]


sorted_moves = sorted(points_per_move, key=me.word_finder_sort)
duplicate_checker = set()
unique_moves = []
for points, move in sorted_moves:
    if move.get_word() not in duplicate_checker:
        duplicate_checker.add(move.get_word())
        unique_moves.append((points, move))

top_40_moves = unique_moves[:40]

for move in top_40_moves:
    print(move)
    print()

print(len(points_per_move))

# word_before, word_after = b.get_surrounding_words(10, 8, False)

# print(word_before, word_after)

# print(g.cross_check(word_before, word_after))



