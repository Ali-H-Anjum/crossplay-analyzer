from Board import Board
from Move import Move
from Tiles import Tiles
from MoveGenerator import MoveGenerator
from gaddagTesting import GADDAG
from MoveEvaluator import MoveEvaluator

g = GADDAG()

mg = MoveGenerator(g)

b = Board()

b.add_move(Move('LANDAU', 7, 12, True))
b.add_move(Move('NUDELY', 6, 7, False))

b.show_board()
me = MoveEvaluator(b)

t = Tiles(['E', 'R', 'S', 'B', 'E', 'E', 'S'])

print(t)
print()

moves = mg.get_all_moves(b, t)
points_per_move = me.sort_by_points(moves)

highest_moves = points_per_move[:15]
most_points, move_with_most_points = highest_moves[0]

word_finder_moves = me.sort_by_word_finder(points_per_move)

for move in word_finder_moves:
    print(move)
    

print(len(points_per_move))

# word_before, word_after = b.get_surrounding_words(10, 8, False)

# print(word_before, word_after)

# print(g.cross_check(word_before, word_after))



