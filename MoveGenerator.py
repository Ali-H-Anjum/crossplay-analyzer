from Move import Move
class MoveGenerator:
    _ALL_LETTERS = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')

    def __init__(self, gaddag):
        self._gaddag = gaddag
        self._root = self._gaddag.get_root()
        

    def get_all_moves(self, board, tiles):
        self._moves = set()
        
        self._board = board
        self._horizontal_constraints, self._vertical_constraints = self._get_all_constraints()

        edge_tiles = board.get_edge_points()
        all_tiles = board.get_all_letters()

        anchors = {(7, 7)} if not edge_tiles else edge_tiles #Seems to be correct but if need be I can add all_tiles back

        for x, y in anchors:
            # print(f"Generating moves for anchor ({x}, {y})")
            # print(f"Horizontal constraints: {self._horizontal_constraints.get((x, y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))}")
            # print(f"Vertical constraints: {self._vertical_constraints.get((x, y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))}")
            # print()

            self._generate_horizontal_moves(x, y, 0, "", tiles, self._root)
            self._generate_vertical_moves(x, y, 0, "", tiles, self._root)

        return self._moves
    
    def _get_all_constraints(self):
        horizontal_constraints = {}
        vertical_constraints = {}

        computed_cols = set()
        computed_rows = set()

        for x, y in self._board.get_edge_points():

            if x not in computed_cols:
                for i in range(15):
                    horizontal_constraints[(x, i)] = self._get_potential_letters(x, i, True)
                computed_cols.add(x)

            if y not in computed_rows:
                for i in range(15):
                    vertical_constraints[(i, y)] = self._get_potential_letters(i, y, False)
                computed_rows.add(y)
        
        return horizontal_constraints, vertical_constraints

    def _get_potential_letters(self, x, y, is_descending):
        if letter := self._board.get_letter_at_point(x, y):
            return {letter}

        word_before, word_after = self._board.get_surrounding_words(x, y, is_descending)

        if not word_before and not word_after:
            return self._ALL_LETTERS

        return self._gaddag.cross_check(word_before, word_after)
    
    def _generate_horizontal_moves(self, x, y, x_offset, word, tiles, path, placed = False):
        current_x = x + x_offset

        current_letter = self._board.get_letter_at_point(current_x, y)

        if current_letter:
            next_path = self._gaddag.next_arc(path, current_letter)
            if next_path is not None:
                self._horizontal_go_on(x, y, x_offset, current_letter, word, tiles, next_path, placed)

        elif tiles:
            current_vertical_constraints = self._vertical_constraints.get((current_x, y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))

            for tile in tiles.get_unique_tiles():
                if tile not in current_vertical_constraints:
                    continue

                next_path = self._gaddag.next_arc(path, tile)
                if next_path is not None:
                    tiles.remove_tile(tile)
                    self._horizontal_go_on(x, y, x_offset, tile, word, tiles, next_path, placed = True)
                    tiles.restore_tile(tile)

            if tiles.get_blank_count():
                for allowed_letter in current_vertical_constraints: #Blanks get passed through as lowercase, won't be seen when calculating points but will be uppercase when added to board
                    next_path = self._gaddag.next_arc(path, allowed_letter)
                    if next_path is not None:
                        tiles.remove_tile('?')
                        self._horizontal_go_on(x, y, x_offset, allowed_letter.lower(), word, tiles, next_path, placed = True)
                        tiles.restore_tile('?')

    def _horizontal_go_on(self, x, y, x_offset, letter, word, tiles, new_path, placed = False):
        current_x = x + x_offset

        if x_offset <= 0:
            word = letter + word
            letter_to_the_left = self._board.get_letter_at_point(current_x - 1, y)
            letter_to_the_right_of_anchor = self._board.get_letter_at_point(x + 1, y)

            if new_path is not None:
                if new_path.is_terminal and not letter_to_the_left and not letter_to_the_right_of_anchor and placed:
                    self._moves.add(Move(word, current_x, y, False))

                if current_x > 0:
                    self._generate_horizontal_moves(x, y, x_offset - 1, word, tiles, new_path, placed)

                turn_path = self._gaddag.next_arc(new_path, '^') #Basicaly checks if there is a child node with '^' and traverses to it
                if turn_path is not None and not letter_to_the_left and x < 14: #Technically, doesn't need a check for if theres room because I already have a method to check if the position is in bounds
                    self._generate_horizontal_moves(x, y, 1, word, tiles, turn_path, placed)
              
        else:
            word = word + letter
            letter_to_the_right = self._board.get_letter_at_point(current_x + 1, y)

            if new_path is not None:
                if not letter_to_the_right and new_path.is_terminal and placed:
                    self._moves.add(Move(word, current_x - len(word) + 1, y, False))

                if current_x < 14: #Doesn't need check
                    self._generate_horizontal_moves(x, y, x_offset + 1, word, tiles, new_path, placed)
   
    def _generate_vertical_moves(self, x, y, y_offset, word, tiles, path, placed = False):
        current_y = y + y_offset
        
        current_letter = self._board.get_letter_at_point(x, current_y)

        if current_letter:
            next_path = self._gaddag.next_arc(path, current_letter)
            if next_path is not None:
                self._vertical_go_on(x, y, y_offset, current_letter, word, tiles, next_path, placed)
        
        elif tiles:
            current_horizontal_constraints = self._horizontal_constraints.get((x, current_y), set('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))

            for tile in tiles.get_unique_tiles():
                if tile not in current_horizontal_constraints:
                    continue

                next_path = self._gaddag.next_arc(path, tile)
                if next_path is not None:
                    tiles.remove_tile(tile)
                    self._vertical_go_on(x, y, y_offset, tile, word, tiles, next_path, placed = True)
                    tiles.restore_tile(tile)

            if tiles.get_blank_count():
                for allowed_letter in current_horizontal_constraints:
                    next_path = self._gaddag.next_arc(path, allowed_letter)
                    if next_path is not None:
                        tiles.remove_tile('?')
                        self._vertical_go_on(x, y, y_offset, allowed_letter.lower(), word, tiles, next_path, placed = True)
                        tiles.restore_tile('?')

    def _vertical_go_on(self, x, y, y_offset, letter, word, tiles, new_path, placed = False):
        current_y = y + y_offset

        if y_offset >= 0:
            word = letter + word
            letter_above = self._board.get_letter_at_point(x, current_y + 1)
            letter_below_anchor = self._board.get_letter_at_point(x, y - 1)

            if new_path is not None:
                if new_path.is_terminal and not letter_above and not letter_below_anchor and placed:
                    self._moves.add(Move(word, x, current_y, True))

                if current_y < 14:
                    self._generate_vertical_moves(x, y, y_offset + 1, word, tiles, new_path, placed)

                turn_path = self._gaddag.next_arc(new_path, '^')
                if turn_path is not None and not letter_above and y > 0:
                    self._generate_vertical_moves(x, y, -1, word, tiles, turn_path, placed)

        else:
            word = word + letter
            letter_below = self._board.get_letter_at_point(x, current_y - 1)

            if new_path is not None:
                if not letter_below and new_path.is_terminal and placed:
                    self._moves.add(Move(word, x, current_y + len(word) - 1, True))
                
                if current_y > 0:
                    self._generate_vertical_moves(x, y, y_offset - 1, word, tiles, new_path, placed)     