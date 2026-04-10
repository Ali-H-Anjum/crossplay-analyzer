class Board:
    def __init__(self):
        self._board = [[' ' for _ in range(15)] for _ in range(15)]
        self._letter_points = set()
        self._edge_points = set()

        self._rows_with_letters = set()
        self._cols_with_letters = set()

        self._words_placed = []

    def get_board(self): return self._board

    def show_board(self):
        for row in self._board:
            print(' | '.join(row))

    def in_bounds(self, x, y): return 0 <= x <= 14 and 0 <= y <= 14

    def add_letter(self, letter, x, y):
        self._board[14-y][x] = letter
        self._letter_points.add((letter, x, y)) #Points like Coord

        self._rows_with_letters.add(y)
        self._cols_with_letters.add(x)
        
        self._edge_points.discard((x, y))

        if((self.in_bounds(x + 1, y)) and (not self.get_letter_at_point(x + 1, y))):
            self._edge_points.add((x + 1, y))

        if((self.in_bounds(x - 1, y)) and (not self.get_letter_at_point(x - 1, y))):
            self._edge_points.add((x - 1, y))

        if((self.in_bounds(x, y + 1)) and (not self.get_letter_at_point(x, y + 1))):
            self._edge_points.add((x, y + 1))

        if((self.in_bounds(x, y - 1)) and (not self.get_letter_at_point(x, y - 1))):
            self._edge_points.add((x, y - 1))    

    def add_word(self, word, x, y, is_descending):
        self._words_placed.append((word, x, y, is_descending))

        for letter in word:
            self.add_letter(letter, x, y)
            if (is_descending): y = y - 1
            else: x = x + 1

    def get_letter_at_point(self, x, y):
        if not self.in_bounds(x, y): return None
        if self._board[14 - y][x] == ' ': return None

        return self._board[14 - y][x]
        
    def get_edge_points(self): return self._edge_points
    
    def get_all_letters(self): return self._letter_points
    
    def get_rows_with_letters(self): return sorted(self._rows_with_letters)
    
    def get_cols_with_letters(self): return sorted(self._cols_with_letters)
    
    def get_words_placed(self): return self._words_placed
    
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
    
    def get_potential_letters_at_point(self, x, y, is_descending, gaddag):
        if letter := self.get_letter_at_point(x, y):
            return {letter}
        
        if is_descending:
            word_before = self.get_word_at_point(x - 1, y, False) #word left
            word_after  = self.get_word_at_point(x + 1, y, False) #word right
        else:
            word_before = self.get_word_at_point(x, y + 1, True) #word above
            word_after  = self.get_word_at_point(x, y - 1, True) #word below

        if not word_before and not word_after:
            return set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')
        
        return gaddag.cross_check(word_before, word_after)
    
    def update_all_constraints(self, gaddag):
        self._horizontal_constraints = {} #When doing vertical expansion we use horizontal constraints
        self._vertical_constraints = {} #When doing horizontal expansion we use vertical constraints

        computed_cols = set()
        computed_rows = set()
        
        for x, y in self._edge_points:
            if x not in computed_cols:
                for i in range(15):
                    self._horizontal_constraints[(x, i)] = self.get_potential_letters_at_point(x, i, True, gaddag)
                computed_cols.add(x)

            if y not in computed_rows:
                for i in range(15):
                    self._vertical_constraints[(i, y)] = self.get_potential_letters_at_point(i, y, False, gaddag)
                computed_rows.add(y)

    def get_horizontal_constraints(self): return self._horizontal_constraints

    def get_vertical_constraints(self): return self._vertical_constraints
    