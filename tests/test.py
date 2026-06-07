from board import Board
from move import Move
from tray import Tray
from move_generator import MoveGenerator
from gaddag import GADDAG, GADDAGNode
from move_evaluator import MoveEvaluator

import cProfile

def main():
    g = GADDAG()
    mg = MoveGenerator(g)
    b = Board()
    b.add_move(Move('LANDAU', 7, 12, True))
    b.add_move(Move('NUDELY', 6, 7, False))
    b.add_move(Move('BESES', 5, 6, False))
    b.show_board()
    

    # for row in b.get_transposed():
    #     print(' | '.join(cell if cell is not None else ' ' for cell in row))
    # print()

    # print()

    # c = Board()
    # c.set_board(b.get_board())
    # c.show_board()


    me = MoveEvaluator(b)

    t = Tray(['E', 'R', 'S', 'B', 'E', 'E', 'S'])

    print(t)
    print()

    moves = mg.get_all_moves(b.snapshot(), t.snapshot())
    points_per_move = me.sort_by_points(moves)

    # highest_moves = points_per_move[:15]
    # most_points, move_with_most_points = highest_moves[0]

    word_finder_moves = me.sort_by_word_finder(points_per_move)

    for move in word_finder_moves:
        print(move)
        

    print(len(moves))

    # word_before, word_after = b.get_surrounding_words(10, 8, False)

    # print(word_before, word_after)

    # print(g.cross_check(word_before, word_after))

    



if __name__ == "__main__":
    cProfile.run('main()', sort = 'cumtime')