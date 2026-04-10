from Move import Move
from gaddagTesting import GADDAG
from Board import Board
from Tiles import Tiles

#####################################################################
def generate_horizontal_moves(board, vertical_constraints, x, y, x_offset, word, tiles, path, moves):
    current_x = x + x_offset

    if not board.in_bounds(current_x, y):
        return

    current_letter = board.get_letter_at_point(current_x, y)

    if current_letter:
        next_path = g.next_arc(path, current_letter)
        if next_path != 0:
            horizontal_go_on(board, vertical_constraints, x, y, x_offset, current_letter, word, tiles, next_path, moves)

    elif tiles:
        current_vertical_constraints = vertical_constraints.get((current_x, y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))

        for tile in tiles.get_unique_tiles():

            if tile not in current_vertical_constraints:
                continue

            next_path = g.next_arc(path, tile)
            if next_path != 0:
                horizontal_go_on(board, vertical_constraints, x, y, x_offset, tile, word, tiles.remove_tile(tile), next_path, moves)

        if tiles.get_blank_count():
            tiles_without_blank = tiles.remove_tile('?')
            for allowed_letter in current_vertical_constraints:
                next_path = g.next_arc(path, allowed_letter)
                if next_path != 0:
                    horizontal_go_on(board, vertical_constraints, x, y, x_offset, allowed_letter, word, tiles_without_blank, next_path, moves)

def horizontal_go_on(board, vertical_constraints, x, y, x_offset, letter, word, tiles, new_path, moves):
    current_x = x + x_offset

    if x_offset <= 0:
        word = letter + word
        letter_to_the_left = board.get_letter_at_point(current_x - 1, y)

        if new_path != 0 and current_x > 0:
            generate_horizontal_moves(board, vertical_constraints, x, y, x_offset - 1, word, tiles, new_path, moves)

            turn_path = g.next_arc(new_path, 0)
            if turn_path != 0:
                if not letter_to_the_left:
                    if turn_path.is_terminal:
                        moves.add((word, current_x, y, False))

                    if current_x <= 14:
                        generate_horizontal_moves(board, vertical_constraints, x, y, 1, word, tiles, turn_path, moves)

                        
    else:
        word = word + letter
        letter_to_the_right = board.get_letter_at_point(current_x + 1, y)

        if new_path != 0:
            if not letter_to_the_right and new_path.is_terminal:
                moves.add((word, current_x - len(word) + 1, y, False))

            if current_x <= 14:
                generate_horizontal_moves(board, vertical_constraints, x, y, x_offset + 1, word, tiles, new_path, moves)

def generate_vertical_moves(board, horizontal_constraints, x, y, y_offset, word, tiles, path, moves):
    current_y = y + y_offset

    if not board.in_bounds(x, current_y):
        return
    
    current_letter = board.get_letter_at_point(x, current_y)

    if current_letter:
        next_path = g.next_arc(path, current_letter)
        if next_path != 0:
            vertical_go_on(board, horizontal_constraints, x, y, y_offset, current_letter, word, tiles, next_path, moves)
     
    elif tiles:
        current_horizontal_constraints = horizontal_constraints.get((x, current_y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))

        for tile in tiles.get_unique_tiles():
            if tile not in current_horizontal_constraints:
                continue

            next_path = g.next_arc(path, tile)
            if next_path != 0:
                vertical_go_on(board, horizontal_constraints, x, y, y_offset, tile, word, tiles.remove_tile(tile), next_path, moves)

        if tiles.get_blank_count():
            tiles_without_blank = tiles.remove_tile('?')
            for allowed_letter in current_horizontal_constraints:
                next_path = g.next_arc(path, allowed_letter)
                if next_path != 0:
                    vertical_go_on(board, horizontal_constraints, x, y, y_offset, allowed_letter, word, tiles_without_blank, next_path, moves)

def vertical_go_on(board, horizontal_constraints, x, y, y_offset, letter, word, tiles, new_path, moves):
    current_y = y + y_offset

    if y_offset >= 0:
        word = letter + word
        letter_above = board.get_letter_at_point(x, current_y + 1)

        if new_path != 0 and current_y < 14:
            generate_vertical_moves(board, horizontal_constraints, x, y, y_offset + 1, word, tiles, new_path, moves)

            turn_path = g.next_arc(new_path, 0)
            if turn_path != 0:
                if not letter_above:
                    if turn_path.is_terminal:
                        moves.add((word, x, current_y, True))

                    if current_y >= 0:
                        generate_vertical_moves(board, horizontal_constraints, x, y, -1, word, tiles, turn_path, moves)

    else:
        word = word + letter
        letter_below = board.get_letter_at_point(x, current_y - 1)

        if new_path != 0:
            if not letter_below and new_path.is_terminal:
                moves.add((word, x, current_y + len(word) - 1, True))
            
            if current_y >= 0:
                generate_vertical_moves(board, horizontal_constraints, x, y, y_offset - 1, word, tiles, new_path, moves)

#####################################################################

g = GADDAG()

print(g.contains_whole_word('OJ')) #Should be false
            
b = Board()

m0 = Move('HELLO', 7, 0, False)
m1 = Move('BRAILE', 8, 5, True)
m2 = Move('JELLO', 11, 4, True)

#b.add_word(m0.get_word(), m0.get_x(), m0.get_y(), m0.get_is_descending())
#b.add_word(m1.get_word(), m1.get_x(), m1.get_y(), m1.get_is_descending())
#b.add_word(m2.get_word(), m2.get_x(), m2.get_y(), m2.get_is_descending())

b.add_word('HAJ', 7, 7, True)

b.update_all_constraints(g)

#print('\n', 6, 0,'\n', b.get_horizontal_constraints()[6, 0], b.get_vertical_constraints()[(6, 0)], '\n')


##########################################################################
    
t = Tiles(['A', 'L', 'I', 'G', 'D', 'H', 'I'])

#We have to expand horizontally and then vertically from each edge
#When doing vertical expansion we use horizontal constraints
#When doing horizontal expansion we use vertical constraints

edge_tiles = b.get_edge_points()

all_tiles = b.get_all_letters()

horizontal_constraints = b.get_horizontal_constraints() 

vertical_constraints = b.get_vertical_constraints()

b.show_board()

#############################################################################

moves = set()

if not edge_tiles:
    edge_tiles = {(7, 7)}

for x, y in edge_tiles:
    generate_horizontal_moves(b, vertical_constraints, x, y, 0, "", t, g.get_root(), moves)
    generate_vertical_moves(b, horizontal_constraints, x, y, 0, "", t, g.get_root(), moves)

for let, x, y in all_tiles:
    generate_horizontal_moves(b, vertical_constraints, x, y, 0, "", t, g.get_root(), moves)
    generate_vertical_moves(b, horizontal_constraints, x, y, 0, "", t, g.get_root(), moves)

print(horizontal_constraints.get((7, 4)), vertical_constraints.get((7, 4)))
print(moves, len(moves))



