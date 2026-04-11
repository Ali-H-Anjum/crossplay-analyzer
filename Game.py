from Move import Move
from gaddagTesting import GADDAG
from Board import Board
from Tiles import Tiles
from MoveGenerator import MoveGenerator

g = GADDAG()

mg = MoveGenerator(g)
            
b = Board()

t = Tiles(['A', 'L', 'I', 'G', 'D', 'H', 'I'])

#b.add_word('HAJ', 7, 7, True)
#m0 = Move('HELLO', 7, 0, False)
#m1 = Move('BRAILE', 8, 5, True)
#m2 = Move('JELLO', 11, 4, True)
#b.add_word(m0.get_word(), m0.get_x(), m0.get_y(), m0.get_is_descending())
#b.add_word(m1.get_word(), m1.get_x(), m1.get_y(), m1.get_is_descending())
#b.add_word(m2.get_word(), m2.get_x(), m2.get_y(), m2.get_is_descending())

#We have to expand horizontally and then vertically from each edge
#When doing vertical expansion we use horizontal constraints
#When doing horizontal expansion we use vertical constraints



moves = mg.get_all_moves(b, t)


b.show_board()
print(moves, len(moves))



