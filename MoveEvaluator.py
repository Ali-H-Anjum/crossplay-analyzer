points_per_tile = {'?': 0, 'A': 1, 'B': 4, 'C': 3, 'D': 2, 'E': 1, 'F': 4, 'G': 4, 'H': 3,'I': 1, 'J': 10, 'K': 6, 'L': 2, 'M': 3,
                    'N': 1, 'O': 1, 'P': 3, 'Q': 10,'R': 1, 'S': 1, 'T': 1, 'U': 2, 'V': 6, 'W': 5, 'X': 8, 'Y': 4, 'Z': 10}

class MoveEvaluator:

    def __init__(self, board):
        self._board = board

    def set_board(self, board):
        self._board = board

    def calculate_points_per_move(self, move):
        word, x, y, is_descending = move.get_word(), move.get_x(), move.get_y(), move.get_is_descending()
        DW = self._board.get_double_word_multipliers()
        TW = self._board.get_triple_word_multipliers()
        
        n = 1
        points = 0

        for i in word:
            if (x, y) in DW:
                n *= 2
            elif (x, y) in TW:
                n *= 3

            points += self._calculate_points_per_letter(i, x, y)

            if is_descending:
                y = y - 1
            else:
                x = x + 1
            
        return points * n

    def _calculate_points_per_letter(self, letter, x, y):

        DL = self._board.get_double_letter_multipliers()
        TL = self._board.get_triple_letter_multipliers()

        m = 1
        if (x, y) in DL:
            m = m + 1

        elif (x, y) in TL:
            m = m + 2

        return points_per_tile.get(letter, 0) * m
    
    def sort_by_points(self, moves):
        points_per_move = [(self.calculate_points_per_move(move), move) for move in moves]
        points_per_move.sort(reverse=True)
        return points_per_move

    def get_top_number(self, moves, number):
        return moves[:number]


