from tray import Tray

class Player: 
    def __init__(self, tiles):
        self._tiles = Tray(tiles)
        self._score = 0

    def get_player_tiles(self): 
        return self._tiles
    
    def add_tiles(self, tiles):
        self._tiles.add_tiles(tiles)

    def remove_tile(self, tile): 
        self._tiles.remove_tile(tile)

    def tiles_needed(self):
        return 7 - len(self._tiles)

    def add_score(self, score): 
        self._score += score

    def get_score(self):
        return self._score
    
    def __len__(self):
        return len(self._tiles)
    

    ##################### AI METHODS #####################

    def snapshot(self):
        return (
            self._tiles.snapshot(), 
            self._score
        )
    
    def restore(self, player_snapshot):
        tiles_snapshot, self._score = player_snapshot
        self._tiles.restore(tiles_snapshot)

    
