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