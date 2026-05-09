from move import Move
from board import Board

points_per_tile = {'?': 0, 'A': 1, 'B': 4, 'C': 3, 'D': 2, 'E': 1, 'F': 4, 'G': 4, 'H': 3,'I': 1, 'J': 10, 'K': 6, 'L': 2, 'M': 3,
                    'N': 1, 'O': 1, 'P': 3, 'Q': 10,'R': 1, 'S': 1, 'T': 1, 'U': 2, 'V': 6, 'W': 5, 'X': 8, 'Y': 4, 'Z': 10}

MULT_NONE, MULT_DL, MULT_TL, MULT_DW, MULT_TW = 0, 1, 2, 3, 4

class MoveEvaluator:
    _MULT = {
        # DL = 1
        (9,7):1,(7,9):1,(5,7):1,(7,5):1,(14,7):1,(7,14):1,(0,7):1,(7,0):1,
        (12,10):1,(11,11):1,(10,12):1,(4,12):1,(3,11):1,(2,10):1,
        (2,4):1,(3,3):1,(4,2):1,(10,2):1,(11,3):1,(12,4):1,
        # TL = 2
        (10,9):2,(9,10):2,(5,10):2,(4,9):2,(4,5):2,(5,4):2,(9,4):2,(10,5):2,
        (13,6):2,(13,8):2,(8,13):2,(6,13):2,(1,8):2,(1,6):2,(6,1):2,(8,1):2,
        (14,0):2,(14,14):2,(0,14):2,(0,0):2,
        # DW = 3
        (11,7):3,(7,11):3,(3,7):3,(7,3):3,
        (13,1):3,(13,13):3,(1,13):3,(1,1):3,
        # TW = 4
        (14,11):4,(11,14):4,(3,14):4,(0,11):4,(0,3):4,
        (3,0):4,(11,0):4,(14,3):4
    }

    def __init__(self, board: Board):
        self._board = board

    def set_board(self, board: Board):
        self._board = board

    def get_multiplier(self, x: int, y: int):
        return self._MULT.get((x, y), 0)

    def calculate_total_points_per_move(self, move: Move):
        points, tiles_used = self.calculate_points_from_main_word(move)
        points += self.calculate_points_for_all_cross_moves(move)
        points += self.calculate_sweep(tiles_used)
        return points

    def calculate_points_from_main_word(self, move: Move):       
        blank_tiles = self._board.get_blank_positions()
        
        n = 1
        points = 0
        tiles_used = 0

        for letter, x, y in move.get_letter_positions():
            is_blank = letter.islower() or (x, y) in blank_tiles

            if not self._board.get_letter_at_point(x, y):
                tiles_used += 1
                mult = self.get_multiplier(x, y)

                if mult == MULT_DW: n *= 2
                elif mult == MULT_TW: n *= 3
                
                if not is_blank:
                    if mult == MULT_DL: points += points_per_tile[letter.upper()] * 2
                    elif mult == MULT_TL: points += points_per_tile[letter.upper()] * 3
                    else: points += points_per_tile.get(letter.upper(), 0)
            else:
                if not is_blank:
                    points += points_per_tile.get(letter.upper(), 0)

        return points * n, tiles_used
    
    def calculate_points_for_all_cross_moves(self, move: Move):
        total = 0

        for letter, x, y in move.get_letter_positions():
            if not self._board.get_letter_at_point(x, y):
                total += self.calculate_points_per_cross(letter, x, y, move.get_is_descending())


        return total

    def calculate_points_per_cross(self, letter: chr, x: int, y: int, is_descending: bool):
        word_before, word_after = self._board.get_surrounding_words(x, y, is_descending)

        if not word_before and not word_after:
            return 0
        
        cross_word = word_before + letter + word_after

        if is_descending:
            cross_x = x - len(word_before)
            cross_y = y
            cross_is_descending = False
        else:
            cross_x = x
            cross_y = y + len(word_before)
            cross_is_descending = True

        cross_move = Move(cross_word, cross_x, cross_y, cross_is_descending)
        points, _ = self.calculate_points_from_main_word(cross_move)
        return points
    
    def calculate_sweep(self, tiles_used: int):
        if tiles_used == 7:
            return 40
        return 0

    def sort_by_points(self, moves):
        points_per_move = [(self.calculate_total_points_per_move(move), move) for move in moves]
        points_per_move.sort(reverse=True)
        return points_per_move
    
    def word_finder_sort(self, move_tuple):
        points, move = move_tuple
        return (-points, move.get_word(), -move.get_is_descending(), -move.get_x(), -move.get_y())
    
    def sort_by_word_finder(self, moves):
        sorted_moves = sorted(moves, key=self.word_finder_sort)
        duplicate_checker = set()
        unique_moves = []
        for points, move in sorted_moves:
            if move.get_word().upper() not in duplicate_checker:
                duplicate_checker.add(move.get_word().upper())
                unique_moves.append((points, move))

        return unique_moves[:40]