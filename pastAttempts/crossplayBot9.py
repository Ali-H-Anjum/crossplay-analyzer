from collections import Counter
from Board import Board
from Move import Move

def create_word_dictionary(): #turns my file into a dictionary of signatures (sorted words) : words. (Reduces search by 20k lines)
    word_dictionary = {}
    with open("NWL2023-Playability.txt", 'r') as file:
        for line in file:
            parts = line.strip().split()
            
            if len(parts) >= 2:
                word = parts[1].lower()
                signature = ''.join(sorted(word))

                if signature in word_dictionary:
                    word_dictionary[signature].add(word)
                else:
                    word_dictionary[signature] = {word}

    return word_dictionary
 
def searching_for_tiles_in_lane(word_dictionary, preprocessed_tiles, board):
    board.show_board()

    legal_moves = []

    tiles = [t.lower() for t in preprocessed_tiles]
    number_of_tiles = len(tiles)

    tile_counts = Counter(tiles)
    blank_count = tile_counts.get('?', 0)
    blanks_needed = 0

    #for signature, potential_expansions in word_dictionary.items():

    #print(board.get_rows_with_letters(), board.get_cols_with_letters())
    #print(board.get_letter_at_point(9, 0))
    #print(board.get_all_letters())
    #print(board.get_edge_positions(), len(board.get_edge_positions()))
    #print(board.get_edge_positions_and_direction(), len(board.get_edge_positions_and_direction()))

    edge_possibilities = {}

    for edge in board.get_edge_positions_and_direction():
        x, y, direction = edge
        possible_letters = set()

        if (direction == 'left'):
            adjacent_letter = board.get_letter_at_point(x + 1, y)
            concat = lambda l, a: l + a

        if (direction == 'right'):
            adjacent_letter = board.get_letter_at_point(x - 1, y)
            concat = lambda l, a: a + l

        if (direction == 'up'):
            adjacent_letter = board.get_letter_at_point(x, y - 1)
            concat = lambda l, a: l + a

        if (direction == 'down'):
            adjacent_letter = board.get_letter_at_point(x, y + 1)
            concat = lambda l, a: a + l

        for letter in 'abcdefghijklmnopqrstuvwxyz':
            check = concat(letter, adjacent_letter)
            check_signature = ''.join(sorted(check))

            if check_signature in word_dictionary:
                if check in word_dictionary[check_signature]:
                    possible_letters.add(letter)

        key = (x, y)
        if key not in edge_possibilities:
            edge_possibilities[key] = possible_letters

        edge_possibilities[key].update(set.intersection(possible_letters, edge_possibilities[key]))
       
    print(edge_possibilities)

            

#############################################
wd = create_word_dictionary()
t = ['A', 'L', 'I', 'G', 'D', 'H', 'I']
b = Board()

m0 = Move('hello', 7, 0, False)
m1 = Move('braile', 8, 5, True)
m2 = Move('jello', 11, 4, True)

b.add_move(m0.get_word(), m0.get_x(), m0.get_y(), m0.get_is_descending())
b.add_move(m1.get_word(), m1.get_x(), m1.get_y(), m1.get_is_descending())
b.add_move(m2.get_word(), m2.get_x(), m2.get_y(), m2.get_is_descending())

searching_for_tiles_in_lane(wd, t, b)