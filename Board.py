class Board:
    def __init__(self):
        self._board = [[' ' for _ in range(15)] for _ in range(15)]
        self._letter_points = set()
        self._edge_points = set()

        self._words_placed = []

        self._horizontal_constraints = {}
        self._vertical_constraints = {}

    def get_board(self): return self._board

    def show_board(self):
        for row in self._board:
            print(' | '.join(row))

    def in_bounds(self, x, y): return 0 <= x <= 14 and 0 <= y <= 14

    def _add_letter(self, letter, x, y):
        self._board[14-y][x] = letter
        self._letter_points.add((x, y)) #Points like Coord
        
        self._edge_points.discard((x, y))

        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            self._add_edge(x + dx, y + dy)

    def _add_edge(self, x, y):
        if self.in_bounds(x, y) and not self.get_letter_at_point(x, y):
            self._edge_points.add((x, y))    

    def add_word(self, word, x, y, is_descending):
        self._words_placed.append((word, x, y, is_descending))  

        for letter in word:
            self._add_letter(letter, x, y)
            if is_descending: y = y - 1
            else: x = x + 1

    def get_letter_at_point(self, x, y):
        if not self.in_bounds(x, y): return None
        if self._board[14 - y][x] == ' ': return None

        return self._board[14 - y][x]
        
    def get_edge_points(self): return self._edge_points
    
    def get_all_letters(self): return self._letter_points
    
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
    