class Board:
    def __init__(self):
        self._board = [[' ' for _ in range(15)] for _ in range(15)]
        # self._letter_points = set()
        self._edge_points = set()

        # self._moves_made = [] Honestly using this to build the board would probably be better, using a stack here would also make undoing a move a lot easier for simulations

        self._blank_tiles = set()

    def get_board(self): return tuple(tuple(row) for row in self._board) #Immutable

    def set_board(self, tuple_board): #Should overload this in constructor
        self._board = [[' ' for _ in range(15)] for _ in range(15)]
        for i, row in enumerate(tuple_board):
            for j, col in enumerate(row):
                self._add_letter(col, j, 14-i)

    def show_board(self):
        for row in self._board:
            print(' | '.join(row))

    def in_bounds(self, x, y): return 0 <= x <= 14 and 0 <= y <= 14

    def _add_letter(self, letter, x, y):
        if letter.islower():
            self._blank_tiles.add((x, y))

        self._board[14-y][x] = letter.upper() #Necessary for blank tile logic
        # self._letter_points.add((x, y)) #Points like Coord
        
        self._edge_points.discard((x, y))

        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            self._add_edge(x + dx, y + dy)

    def _add_edge(self, x, y):
        if self.in_bounds(x, y) and not self.get_letter_at_point(x, y):
            self._edge_points.add((x, y))    

    def add_move(self, move):
        # self._moves_made.append(move)  

        for letter, x, y in move.get_letter_positions():
            self._add_letter(letter, x, y)

    def get_letter_at_point(self, x, y):
        if not self.in_bounds(x, y): return None
        if self._board[14 - y][x] == ' ': return None

        return self._board[14 - y][x]
        
    def get_edge_points(self): return self._edge_points
    
    # def get_all_letters(self): return self._letter_points
    
    # def get_moves_made(self): return self._moves_made
    
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
    
    def get_blank_tiles(self): return self._blank_tiles

    ###################################################################
