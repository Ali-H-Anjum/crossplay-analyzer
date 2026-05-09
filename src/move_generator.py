from move import Move
from board import Board
from tray import Tray
from gaddag import GADDAG, GADDAGNode

class MoveGenerator:
    _ALL_LETTERS = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')

    def __init__(self, gaddag: GADDAG):
        self._gaddag = gaddag
        self._root = self._gaddag.get_root()
        
    def get_all_moves(self, board: Board, tiles: Tray):
        self._moves = set()
        
        self._board = board
        self._cross_checks = self._compute_all_cross_checks()

        edge_tiles = board.get_anchor_positions()

        anchors = {(7, 7)} if not edge_tiles else edge_tiles #Seems to be correct but if need be I can add all_tiles back

        for x, y in anchors:
            # print(f"Generating moves for anchor ({x}, {y})")
            # print(f"Horizontal constraints: {self._horizontal_constraints.get((x, y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))}")
            # print(f"Vertical constraints: {self._vertical_constraints.get((x, y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))}")
            # print()

            # self._generate_horizontal_moves(x, y, 0, "", tiles, self._root)
            # self._generate_vertical_moves(x, y, 0, "", tiles, self._root)

            self._gen(x, y, 0, "", tiles, self._root, False, is_vertical=False)
            self._gen(x, y, 0, "", tiles, self._root, False, is_vertical=True)

        return self._moves
    
    def _compute_all_cross_checks(self):
        checks = {}

        computed_cols = set()
        computed_rows = set()

        for x, y in self._board.get_anchor_positions():

            if x not in computed_cols:
                for i in range(15):
                    checks[(x, i, True)] = self._get_potential_letters(x, i, is_descending=True)
                computed_cols.add(x)

            if y not in computed_rows:
                for i in range(15):
                    checks[(i, y, False)] = self._get_potential_letters(i, y, is_descending=False)
                computed_rows.add(y)
        
        return checks

    def _get_potential_letters(self, x: int, y: int, is_descending: bool):
        if letter := self._board.get_letter_at_point(x, y):
            return {letter}

        word_before, word_after = self._board.get_surrounding_words(x, y, is_descending)

        if not word_before and not word_after:
            return self._ALL_LETTERS

        return self._gaddag.cross_check(word_before, word_after)
    
    def _pos(self, anchor_x: int, anchor_y: int, offset: int, is_vertical: bool):
        return (anchor_x, anchor_y - offset) if is_vertical else (anchor_x + offset, anchor_y)
    
    def _gen(self, anchor_x: int, anchor_y: int, offset: int, word: str, tiles: Tray, path: GADDAGNode, placed: bool, is_vertical: bool):
        current_x, current_y = self._pos(anchor_x, anchor_y, offset, is_vertical)

        if not (0 <= current_x < 15 and 0 <= current_y < 15):
            return 
        
        current_letter = self._board.get_letter_at_point(current_x, current_y)

        if current_letter: #IF a letter L is already on this square then
            next_path = self._gaddag.next_arc(path, current_letter)
            if next_path is not None:
                self._go_on(anchor_x, anchor_y, offset, current_letter, word, tiles, next_path, placed, is_vertical)

        elif tiles: #ELSE IF letters remain on the rack THEN
            cross = self._cross_checks.get((current_x, current_y, is_vertical), self._ALL_LETTERS)

            for tile in set(tiles.get_unique_tiles()):
                if tile not in cross:
                    continue

                next_path = self._gaddag.next_arc(path, tile)
                if next_path is not None:
                    tiles.remove_tile(tile)
                    try:
                        self._go_on(anchor_x, anchor_y, offset, tile, word, tiles, next_path, True, is_vertical)
                    finally:
                        tiles.add_tile(tile)

            if tiles.get_blank_count():
                for letter in cross:
                    next_path = self._gaddag.next_arc(path, letter)
                    if next_path is not None:
                        tiles.remove_tile('?')
                        try:
                            self._go_on(anchor_x, anchor_y, offset, letter.lower(), word, tiles, next_path, True, is_vertical)
                        finally:
                            tiles.add_tile('?')

    def _go_on(self, anchor_x: int, anchor_y: int, offset: int, letter: chr, word: str, tiles: Tray, new_path: GADDAGNode, placed: bool, is_vertical: bool):

        current_x, current_y = self._pos(anchor_x, anchor_y, offset, is_vertical)

        back_of_current_x, back_of_current_y = self._pos(anchor_x, anchor_y, offset - 1, is_vertical)

        front_of_anchor_x, front_of_anchor_y = self._pos(anchor_x, anchor_y, 1, is_vertical)

        if offset <= 0:
            word = letter + word
            no_letter_directly_back = not self._board.get_letter_at_point(back_of_current_x, back_of_current_y)
            no_letter_in_front_of_anchor = not self._board.get_letter_at_point(front_of_anchor_x, front_of_anchor_y)

            if new_path.is_terminal and no_letter_directly_back and no_letter_in_front_of_anchor and placed:
                self._moves.add(Move(word, current_x, current_y, is_vertical))

            if 0 <= back_of_current_x < 15 and 0 <= back_of_current_y < 15:
                self._gen(anchor_x, anchor_y, offset - 1, word, tiles, new_path, placed, is_vertical)

            turn_path = self._gaddag.next_arc(new_path, '^')
            if turn_path is not None and no_letter_directly_back and 0 <= front_of_anchor_x < 15 and 0 <= front_of_anchor_y < 15:
                self._gen(anchor_x, anchor_y, 1, word, tiles, turn_path, placed, is_vertical)

        else:
            word = word + letter
            front_of_current_x, front_of_current_y = self._pos(anchor_x, anchor_y, offset + 1, is_vertical)  # one step further after
            no_letter_directly_front = not self._board.get_letter_at_point(front_of_current_x, front_of_current_y)

            if no_letter_directly_front and new_path.is_terminal and placed:
                sx, sy = self._pos(anchor_x, anchor_y, offset - len(word) + 1, is_vertical)
                self._moves.add(Move(word, sx, sy, is_vertical))

            if 0 <= front_of_current_x < 15 and 0 <= front_of_current_y < 15:
                self._gen(anchor_x, anchor_y, offset + 1, word, tiles, new_path, placed, is_vertical)
        


    # def _generate_horizontal_moves(self, x, y, x_offset, word, tiles, path, placed = False):
    #     current_x = x + x_offset

    #     current_letter = self._board.get_letter_at_point(current_x, y)

    #     if current_letter:
    #         next_path = self._gaddag.next_arc(path, current_letter)
    #         if next_path is not None:
    #             self._horizontal_go_on(x, y, x_offset, current_letter, word, tiles, next_path, placed)

    #     elif tiles:
    #         current_vertical_constraints = self._cross_checks.get((current_x, y, False), self._ALL_LETTERS)

    #         for tile in set(tiles.get_unique_tiles()):
    #             if tile not in current_vertical_constraints:
    #                 continue

    #             next_path = self._gaddag.next_arc(path, tile)
    #             if next_path is not None:
    #                 tiles.remove_tile(tile)
    #                 try:
    #                     self._horizontal_go_on(x, y, x_offset, tile, word, tiles, next_path, placed = True)
    #                 finally:
    #                     tiles.restore_tile(tile)

    #         if tiles.get_blank_count():
    #             for allowed_letter in current_vertical_constraints: #Blanks get passed through as lowercase, won't be seen when calculating points but will be uppercase when added to board
    #                 next_path = self._gaddag.next_arc(path, allowed_letter)
    #                 if next_path is not None:
    #                     tiles.remove_tile('?')
    #                     try:
    #                         self._horizontal_go_on(x, y, x_offset, allowed_letter.lower(), word, tiles, next_path, placed = True)
    #                     finally:
    #                         tiles.restore_tile('?')

    # def _horizontal_go_on(self, x, y, x_offset, letter, word, tiles, new_path, placed = False):
    #     current_x = x + x_offset

    #     if x_offset <= 0:
    #         word = letter + word
    #         letter_to_the_left = self._board.get_letter_at_point(current_x - 1, y)
    #         letter_to_the_right_of_anchor = self._board.get_letter_at_point(x + 1, y)

    #         if new_path is not None:
    #             if new_path.is_terminal and not letter_to_the_left and not letter_to_the_right_of_anchor and placed:
    #                 self._moves.add(Move(word, current_x, y, False))

    #             if current_x > 0:
    #                 self._generate_horizontal_moves(x, y, x_offset - 1, word, tiles, new_path, placed)

    #             turn_path = self._gaddag.next_arc(new_path, '^') #Basicaly checks if there is a child node with '^' and traverses to it
    #             if turn_path is not None and not letter_to_the_left and x < 14: #Technically, doesn't need a check for if theres room because I already have a method to check if the position is in bounds
    #                 self._generate_horizontal_moves(x, y, 1, word, tiles, turn_path, placed)
              
    #     else:
    #         word = word + letter
    #         letter_to_the_right = self._board.get_letter_at_point(current_x + 1, y)

    #         if new_path is not None:
    #             if not letter_to_the_right and new_path.is_terminal and placed:
    #                 self._moves.add(Move(word, current_x - len(word) + 1, y, False))

    #             if current_x < 14: #Doesn't need check
    #                 self._generate_horizontal_moves(x, y, x_offset + 1, word, tiles, new_path, placed)
   
    # def _generate_vertical_moves(self, x, y, y_offset, word, tiles, path, placed = False):
    #     current_y = y + y_offset
        
    #     current_letter = self._board.get_letter_at_point(x, current_y)

    #     if current_letter:
    #         next_path = self._gaddag.next_arc(path, current_letter)
    #         if next_path is not None:
    #             self._vertical_go_on(x, y, y_offset, current_letter, word, tiles, next_path, placed)
        
    #     elif tiles:
    #         current_horizontal_constraints = self._cross_checks.get((x, current_y, True), self._ALL_LETTERS)

    #         for tile in set(tiles.get_unique_tiles()):
    #             if tile not in current_horizontal_constraints:
    #                 continue

    #             next_path = self._gaddag.next_arc(path, tile)
    #             if next_path is not None:
    #                 tiles.remove_tile(tile)
    #                 try:
    #                     self._vertical_go_on(x, y, y_offset, tile, word, tiles, next_path, placed = True)
    #                 finally:
    #                     tiles.restore_tile(tile)

    #         if tiles.get_blank_count():
    #             for allowed_letter in current_horizontal_constraints:
    #                 next_path = self._gaddag.next_arc(path, allowed_letter)
    #                 if next_path is not None:
    #                     tiles.remove_tile('?')
    #                     try:
    #                         self._vertical_go_on(x, y, y_offset, allowed_letter.lower(), word, tiles, next_path, placed = True)
    #                     finally:
    #                         tiles.restore_tile('?')

    # def _vertical_go_on(self, x, y, y_offset, letter, word, tiles, new_path, placed = False):
    #     current_y = y + y_offset

    #     if y_offset >= 0:
    #         word = letter + word
    #         letter_above = self._board.get_letter_at_point(x, current_y + 1)
    #         letter_below_anchor = self._board.get_letter_at_point(x, y - 1)

    #         if new_path is not None:
    #             if new_path.is_terminal and not letter_above and not letter_below_anchor and placed:
    #                 self._moves.add(Move(word, x, current_y, True))

    #             if current_y < 14:
    #                 self._generate_vertical_moves(x, y, y_offset + 1, word, tiles, new_path, placed)

    #             turn_path = self._gaddag.next_arc(new_path, '^')
    #             if turn_path is not None and not letter_above and y > 0:
    #                 self._generate_vertical_moves(x, y, -1, word, tiles, turn_path, placed)

    #     else:
    #         word = word + letter
    #         letter_below = self._board.get_letter_at_point(x, current_y - 1)

    #         if new_path is not None:
    #             if not letter_below and new_path.is_terminal and placed:
    #                 self._moves.add(Move(word, x, current_y + len(word) - 1, True))
                
    #             if current_y > 0:
    #                 self._generate_vertical_moves(x, y, y_offset - 1, word, tiles, new_path, placed)     