class Board:
    def __init__(self):
        self._board = [[' ' for _ in range(15)] for _ in range(15)]
        self._letter_points = set()
        self._edge_points = set()

        self._horizontal_constraints = {}
        self._vertical_constraints = {}

        self._moves_made = []

        self._double_letter_multipliers = {(9,7),(7,9),(5,7),(7,5),(14,7),(7,14),(0,7),(7,0),(12,10),(11,11),(10,12),(4,12),(3,11),(2,10),(2,4),(3,3),(4,2),(10,2),(11,3),(12,4)}
        self._triple_letter_multipliers = {(10,9),(9,10),(5,10),(4,9),(4,5),(5,4),(9,4),(10,5),(13,6),(13,8),(8,13),(6,13),(1,8),(1,6),(6,1),(8,1),(14,0),(14,14),(0,14),(0,0)}
        self._double_word_multipliers = {(11,7),(7,11),(3,7),(7,3),(13,1),(13,13),(1,13),(1,1)}
        self._triple_word_multipliers = {(14,11),(11,14),(3,14),(0,11),(0,3),(3,0),(11,0),(14,3)}

        self._blank_tiles = set()

    def get_board(self): return tuple(tuple(row) for row in self._board) #Immutable

    def show_board(self):
        for row in self._board:
            print(' | '.join(row))

    def in_bounds(self, x, y): return 0 <= x <= 14 and 0 <= y <= 14

    def _add_letter(self, letter, x, y):
        if letter.islower():
            self._blank_tiles.add((x, y))

        self._board[14-y][x] = letter.upper() #Necessary for blank tile logic
        self._letter_points.add((x, y)) #Points like Coord
        
        self._edge_points.discard((x, y))

        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            self._add_edge(x + dx, y + dy)

        if((x, y) in self._double_letter_multipliers):
            self._double_letter_multipliers.remove((x, y))
        elif((x, y) in self._triple_letter_multipliers):
            self._triple_letter_multipliers.remove((x, y))
        elif((x, y) in self._double_word_multipliers):
            self._double_word_multipliers.remove((x, y))
        elif((x, y) in self._triple_word_multipliers):
            self._triple_word_multipliers.remove((x, y))

    def _add_edge(self, x, y):
        if self.in_bounds(x, y) and not self.get_letter_at_point(x, y):
            self._edge_points.add((x, y))    

    def add_move(self, move):
        self._moves_made.append(move)  

        for letter, x, y in move.get_letter_positions():
            self._add_letter(letter, x, y)


        # word, x, y, is_descending = move.get_word(), move.get_x(), move.get_y(), move.get_is_descending()
        
        # for letter in word:
        #     self._add_letter(letter, x, y)
        #     if is_descending: y = y - 1
        #     else: x = x + 1

    def get_letter_at_point(self, x, y):
        if not self.in_bounds(x, y): return None
        if self._board[14 - y][x] == ' ': return None

        return self._board[14 - y][x]
        
    def get_edge_points(self): return self._edge_points
    
    def get_all_letters(self): return self._letter_points
    
    def get_moves_made(self): return self._moves_made
    
    def get_word_at_point(self, x, y, is_descending):
        word = []

        if is_descending:
            while y < 14 and self.get_letter_at_point(x, y + 1):
                y += 1
        
        else:
            while x > 0 and self.get_letter_at_point(x - 1, y):
                x -= 1

        while letter := self.get_letter_at_point(x, y):
            word.append(letter)
            if is_descending:
                y -= 1
            else:
                x += 1

        return ''.join(word)
    
    def get_surrounding_words(self, x, y, is_descending):
        if is_descending:
            word_before = self.get_word_at_point(x - 1, y, False) if self.get_letter_at_point(x - 1, y) else ''
            word_after  = self.get_word_at_point(x + 1, y, False) if self.get_letter_at_point(x + 1, y) else ''
        else:
            word_before = self.get_word_at_point(x, y + 1, True) if self.get_letter_at_point(x, y + 1) else ''
            word_after  = self.get_word_at_point(x, y - 1, True) if self.get_letter_at_point(x, y - 1) else ''
        return word_before, word_after
    
    def get_double_letter_multipliers(self): return self._double_letter_multipliers

    def get_triple_letter_multipliers(self): return self._triple_letter_multipliers

    def get_double_word_multipliers(self): return self._double_word_multipliers

    def get_triple_word_multipliers(self): return self._triple_word_multipliers

    def get_blank_tiles(self): return self._blank_tiles

    ###################################################################
