from Move import Move

points_per_tile = {'?': 0, 'A': 1, 'B': 4, 'C': 3, 'D': 2, 'E': 1, 'F': 4, 'G': 4, 'H': 3,'I': 1, 'J': 10, 'K': 6, 'L': 2, 'M': 3,
                    'N': 1, 'O': 1, 'P': 3, 'Q': 10,'R': 1, 'S': 1, 'T': 1, 'U': 2, 'V': 6, 'W': 5, 'X': 8, 'Y': 4, 'Z': 10}

class MoveEvaluator:

    def __init__(self, board):
        self._board = board

    def calculate_total_points_per_move(self, move):
        points, tiles_used = self.calculate_points_from_main_word(move)
        points += self.calculate_points_for_all_cross_moves(move)
        points += self.calculate_sweep(tiles_used)
        return points

    def calculate_points_from_main_word(self, move):
        DW = self._board.get_double_word_multipliers()
        TW = self._board.get_triple_word_multipliers()
        DL = self._board.get_double_letter_multipliers()
        TL = self._board.get_triple_letter_multipliers()
        
        blank_tiles = self._board.get_blank_tiles()
        
        n = 1
        points = 0
        tiles_used = 0

        # word, x, y, is_descending = move.get_word(), move.get_x(), move.get_y(), move.get_is_descending()

        # for letter in word:
        #     is_blank = letter.islower() or (x, y) in blank_tiles

        #     if not self._board.get_letter_at_point(x, y):
        #         tiles_used += 1
        #         if not is_blank:
        #             if (x, y) in DW: n *= 2
        #             elif (x, y) in TW: n *= 3

        #             if (x, y) in DL: points +=  points_per_tile.get(letter.upper(), 0) * 2
        #             elif (x, y) in TL: points += points_per_tile.get(letter.upper(), 0) * 3
        #             else: points += points_per_tile.get(letter.upper(), 0)

        #         else:
        #             if (x, y) in DW: n *= 2
        #             elif (x, y) in TW: n *= 3

        #     else:
        #         if not is_blank:
        #             points += points_per_tile.get(letter.upper(), 0)

        #     if is_descending: y -= 1
        #     else: x += 1

        for letter, x, y in move.get_letter_positions():
            is_blank = letter.islower() or (x, y) in blank_tiles

            if not self._board.get_letter_at_point(x, y):
                tiles_used += 1
                
                if not is_blank:
                    if (x, y) in DW: n *= 2
                    elif (x, y) in TW: n *= 3

                    if (x, y) in DL: points +=  points_per_tile.get(letter.upper(), 0) * 2
                    elif (x, y) in TL: points += points_per_tile.get(letter.upper(), 0) * 3
                    else: points += points_per_tile.get(letter.upper(), 0)

                else:
                    if (x, y) in DW: n *= 2
                    elif (x, y) in TW: n *= 3

            else:
                if not is_blank:
                    points += points_per_tile.get(letter.upper(), 0)

            
        return points * n, tiles_used
    
    def calculate_points_for_all_cross_moves(self, move):
        total = 0
        # word, x, y, is_descending = move.get_word(), move.get_x(), move.get_y(), move.get_is_descending()

        # for letter in word:
        #     if not self._board.get_letter_at_point(x, y):
        #         total += self.calculate_points_per_cross(letter, x, y, is_descending)

        #     if is_descending: y -= 1
        #     else: x += 1

        for letter, x, y in move.get_letter_positions():
            if not self._board.get_letter_at_point(x, y):
                total += self.calculate_points_per_cross(letter, x, y, move.get_is_descending())


        return total

    def calculate_points_per_cross(self, letter, x, y, is_descending):
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
    
    def calculate_sweep(self, tiles_used):
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