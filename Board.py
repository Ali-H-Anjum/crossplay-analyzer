from Move import Move
from collections import Counter
from gaddagTesting import GADDAG, GADDAGNode

class Board:
    def __init__(self):
        self.__board = [[' ' for _ in range(15)] for _ in range(15)]
        self.__letter_points = set()
        self.__edge_points = set()

        self.__rows_with_letters = set()
        self.__cols_with_letters = set()

        self.__words_placed = []

    def get_board(self): return self.__board

    def show_board(self):
        for row in self.__board:
            print(' | '.join(row))

    def in_bounds(self, x, y): return 0 <= x <= 14 and 0 <= y <= 14

    def add_letter(self, letter, x, y):
        self.__board[14-y][x] = letter
        self.__letter_points.add((letter, x, y)) #Points like Coord

        self.__rows_with_letters.add(y)
        self.__cols_with_letters.add(x)
        
        self.__edge_points.discard((x, y))

        if((self.in_bounds(x + 1, y)) and (not self.get_letter_at_point(x + 1, y))):
            self.__edge_points.add((x + 1, y))

        if((self.in_bounds(x - 1, y)) and (not self.get_letter_at_point(x - 1, y))):
            self.__edge_points.add((x - 1, y))

        if((self.in_bounds(x, y + 1)) and (not self.get_letter_at_point(x, y + 1))):
            self.__edge_points.add((x, y + 1))

        if((self.in_bounds(x, y - 1)) and (not self.get_letter_at_point(x, y - 1))):
            self.__edge_points.add((x, y - 1))    

    def add_word(self, word, x, y, is_descending):
        self.__words_placed.append((word, x, y, is_descending))

        for letter in word:
            self.add_letter(letter, x, y)
            if (is_descending): y = y - 1
            else: x = x + 1

    def get_letter_at_point(self, x, y):
        if(self.in_bounds(x, y) and self.__board[14 - y][x] != ' '):
            return self.__board[14 - y][x]
        
    def get_edge_points(self): return self.__edge_points
    
    def get_all_letters(self): return self.__letter_points
    
    def get_rows_with_letters(self): return sorted(self.__rows_with_letters)
    
    def get_cols_with_letters(self): return sorted(self.__cols_with_letters)
    
    def get_words_placed(self): return self.__words_placed
    
    def get_word_at_point(self, x, y, is_descending):
        word = []

        if (is_descending):
            while ((y < 14) and (self.get_letter_at_point(x, y + 1))):
                y = y + 1
        
        else:
            while((x > 0) and (self.get_letter_at_point(x - 1, y))):
                x = x - 1

        while True:
            if (not self.get_letter_at_point(x, y)):
                break

            word.append(self.get_letter_at_point(x, y))

            if (is_descending):
                y = y - 1
            else:
                x = x + 1

        return ''.join(word)
    
    def get_potential_letters_for_vertical_expansion_at_point(self, x, y):
        word_above = self.get_word_at_point(x, y + 1, True)
        word_below = self.get_word_at_point(x, y - 1, True)

        if not word_above and not word_below:
            return set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')
        
        if self.get_letter_at_point(x, y):
            return set(self.get_letter_at_point(x, y))
        
        potential_letters = set()
        for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            full_word = word_above + letter + word_below

            if g.is_valid(full_word):
                potential_letters.add(letter)

        return potential_letters
    
    def get_potential_letters_for_horizontal_expansion_at_point(self, x, y):
        word_left = self.get_word_at_point(x - 1, y, False)
        word_right = self.get_word_at_point(x + 1, y, False)

        if not word_left and not word_right:
            return set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')
        
        if self.get_letter_at_point(x, y):
            return set(self.get_letter_at_point(x, y))
        
        potential_letters = set()
        for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            full_word = word_left + letter + word_right

            if g.is_valid(full_word):
                potential_letters.add(letter)

        return potential_letters
    
    def update_all_constraints(self):
        self.__horizontal_constraints = {} #When doing vertical expansion we use horizontal constraints
        self.__vertical_constraints = {} #When doing horizontal expansion we use vertical constraints

        for edge_point in self.__edge_points:
            x, y = edge_point

            for i in range(15):
                self.__horizontal_constraints[(i, y)] = self.get_potential_letters_for_vertical_expansion_at_point(i, y)
                self.__vertical_constraints[(x, i)] = self.get_potential_letters_for_horizontal_expansion_at_point(x, i)

    def get_horizontal_constraints(self): return self.__horizontal_constraints

    def get_vertical_constraints(self): return self.__vertical_constraints
    
#####################################################


g = GADDAG()
with open('wordList.txt', 'r') as file:
    for line in file:
        for word in line.strip().split():
            g.add_word(word)

#print(g.node_count)
#print(g.is_valid("TEST"))

#print(g.is_valid("TESTILP"))

b = Board()
preprocessed_tiles = ['A', 'L', 'I', 'G', 'D', 'H', 'I']

m0 = Move('HELLO', 7, 0, False)
m1 = Move('BRAILE', 8, 5, True)
m2 = Move('JELLO', 11, 4, True)

b.add_word(m0.get_word(), m0.get_x(), m0.get_y(), m0.get_is_descending())
b.add_word(m1.get_word(), m1.get_x(), m1.get_y(), m1.get_is_descending())
b.add_word(m2.get_word(), m2.get_x(), m2.get_y(), m2.get_is_descending())

b.update_all_constraints()

#print('\n', 6, 0,'\n', b.get_horizontal_constraints()[6, 0], b.get_vertical_constraints()[(6, 0)], '\n')


##########################################################################

tiles = [t.upper() for t in preprocessed_tiles]
number_of_tiles = len(tiles)

tile_counts = Counter(tiles)
blank_count = tile_counts.get('?', 0)
blanks_needed = 0

#We have to expand horizontally and then vertically from each edge

board = b.get_board()

board_tiles = b.get_all_letters()

edge_tiles = b.get_edge_points()

horizontal_constraints = b.get_horizontal_constraints() #When doing vertical expansion we use horizontal constraints

vertical_constraints = b.get_vertical_constraints()  #When doing horizontal expansion we use vertical constraints

b.show_board()

print('\n', tiles, '\n', board_tiles, '\n', edge_tiles)


















# for i in range(15):
#     offset = i - x_
#     Gen(offset, None, tiles, g.root())

# def Gen(offset, expansion, tiles, arc):
#     if b.get_letter_at_point(x_, y_):
#         letter = b.get_letter_at_point(x_, y_)

#         GoOn(offset, letter, expansion, tiles, NextArc(arc, L), arc)

#     elif number_of_tiles > 0:
#         for allowed_letter in b.get_vertical_constraints()[(x_, y_)]:
#             if allowed_letter in tiles:
#                 GoON
#             if blank_count > 0:
#                 for allowed_letter in b.get_vertical_constraints()[(x_, y_)]:
#                     if allowed_letter in tiles:
#                         GoOn



# def GoOn(offset, letter, word, tiles, new_arc, old_arc):
#     if offset <= 0:
