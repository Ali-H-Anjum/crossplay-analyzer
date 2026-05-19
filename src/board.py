from move import Move

class Board:
    def __init__(self):
        self._board = [[None] * 15 for _ in range(15)]
        self._anchor_positions = set()
        self._blank_positions = set()

        # self._moves_made = [] Honestly using this to build the board would probably be better, using a stack here would also make undoing a move a lot easier for simulations
        # self._letter_points = set() Set of all positions with letters 
        
    def show_board(self):
        for row in self._board:
            print(' | '.join(cell if cell is not None else ' ' for cell in row))
        print()

    def in_bounds(self, x: int, y: int): return 0 <= x <= 14 and 0 <= y <= 14

    def add_move(self, move: Move):
        for letter, x, y in move.get_letter_positions():
            self._add_letter(letter, x, y)

        # self._moves_made.append(move)

    def _add_letter(self, letter: str, x: int, y: int):
        if letter.islower():
            self._blank_positions.add((x, y))

        self._board[14-y][x] = letter.upper() #Necessary for blank tile logic
        # self._letter_points.add((x, y)) #Points like Coord
        
        self._anchor_positions.discard((x, y))

        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            self._add_edge(x + dx, y + dy)

    def _add_edge(self, x: int, y: int):
        if self.in_bounds(x, y) and not self.get_letter_at_point(x, y):
            self._anchor_positions.add((x, y))    

    def get_letter_at_point(self, x: int, y: int):
        return self._board[14 - y][x]
        
    def get_word_at_point(self, x: int, y: int, is_descending: bool):
        word = []

        if is_descending:
            while y < 14 and self.get_letter_at_point(x, y + 1):
                y += 1
            
            while y >= 0:
                letter = self.get_letter_at_point(x, y)
                if not letter:
                    break
                word.append(letter)
                y -= 1
        else:
            while x > 0 and self.get_letter_at_point(x - 1, y):
                x -= 1
        
            while x < 15:
                letter = self.get_letter_at_point(x, y)
                if not letter:
                    break
                word.append(letter)
                x += 1

        return ''.join(word)
    
    def get_surrounding_words(self, x: int, y: int, is_descending: bool):
        if is_descending:
            word_before = self.get_word_at_point(x - 1, y, False) if 0 < x - 1 and self.get_letter_at_point(x - 1, y) else ''
            word_after  = self.get_word_at_point(x + 1, y, False) if x + 1 < 14 and self.get_letter_at_point(x + 1, y) else ''
        else:
            word_before = self.get_word_at_point(x, y + 1, True) if y + 1 < 14 and self.get_letter_at_point(x, y + 1) else ''
            word_after  = self.get_word_at_point(x, y - 1, True) if 0 < y - 1 and self.get_letter_at_point(x, y - 1) else ''
        return word_before, word_after
    
    def get_anchor_positions(self): return self._anchor_positions

    def get_blank_positions(self): return self._blank_positions
    
    # def get_all_letters(self): return self._letter_points
    
    # def get_moves_made(self): return self._moves_made


    def get_transposed(self):
        return tuple(tuple(self._board[x][y] for x in range(15))for y in range(15))

    ##################### AI METHODS #####################

    def snapshot(self):
        return (
            tuple(tuple(row) for row in self._board),
            tuple(self._anchor_positions),
            tuple(self._blank_positions)
        )

    def restore(self, board_snapshot): 
        tuple_board, anchors, blanks = board_snapshot

        for i, row in enumerate(tuple_board):
            self._board[i] = list(row)
        self._anchor_positions = set(anchors)
        self._blank_positions = set(blanks)


