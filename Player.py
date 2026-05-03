from tiles import Tiles

class Player: 
    def __init__(self, tiles):
        self._player_tiles = Tiles(tiles)
        self._score = 0

    def get_player_tiles(self):
        return self._player_tiles
    
    def add_tiles(self, tiles):
        self._player_tiles.add_tiles(tiles)

    def remove_tile(self, tile):
        self._player_tiles.remove_tile(tile)

    def add_score(self, score):
        self._score += score

    def get_score(self):
        return self._score
    
    def tiles_needed(self):
        return 7 - len(self._player_tiles)
    
    def __len__(self):
        return len(self._player_tiles)
    
