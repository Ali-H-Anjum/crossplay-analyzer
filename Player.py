class Player: #Holds their tiles, all the moves they made, their score
    def __init__(self, tiles):
        self._player_tiles = tiles
        self._score = 0

    def get_player_tiles(self):
        return self._player_tiles
    
    def add_tiles(self, tiles):
        self._player_tiles.add_tiles(tiles)

    def play_tile(self, tile):
        self._player_tiles.play_tile(tile)

    def add_score(self, score):
        self._score += score

    def get_score(self):
        return self._score
    
    def use_tiles_for_move(self, board, move):
        tiles_used = 0

        # word, x, y, is_descending = move.get_word(), move.get_x(), move.get_y(), move.get_is_descending()

        # for letter in word:
        #     if not board.get_letter_at_point(x, y):
        #         if letter in self._player_tiles.get_tiles():
        #             self.play_tile(letter)
        #         else:
        #             self.play_tile('?')
        #         tiles_used += 1

        #     if is_descending: y -= 1
        #     else: x += 1

        for letter, x, y in move.get_letter_positions():
            if board.get_letter_at_point(x, y):
                continue

            if letter in self._player_tiles.get_tiles():
                self.play_tile(letter)
            else:
                self.play_tile('?')

            tiles_used += 1

        return tiles_used

    def is_sweep(self):
        return len(self._player_tiles) == 0