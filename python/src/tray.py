class Tray:
    def __init__(self, tiles):
        self._tiles = tiles
        self._blank_count = tiles.count('?')
        self._unique_tiles = set(tiles) - {'?'}

    def remove_tile(self, tile): #MG
        self._tiles.remove(tile)
        if tile == '?':
            self._blank_count -= 1
        else:
            if tile not in self._tiles:
                self._unique_tiles.remove(tile)

    def add_tile(self, tile): #MG
        self._tiles.append(tile)  # order doesn't matter for generation
        if tile == '?':
            self._blank_count += 1
        else:
            self._unique_tiles.add(tile)

    def add_tiles(self, tiles): #Game
        for tile in tiles:
            self.add_tile(tile)

    def get_tiles(self): #Game
        return self._tiles

    def get_unique_tiles(self):#MG
        return self._unique_tiles
    
    def get_blank_count(self): #MG
        return self._blank_count
    
    def __str__(self):
        return  "Tiles: " + "".join([str(item) for item in self._tiles])

    def __len__(self):
        return len(self._tiles)
    
    def __bool__(self):
        return bool(self._tiles)
    
    ##################### AI METHODS #####################

    def snapshot(self):
        return tuple(self._tiles)
    
    def restore(self, tiles_snapshot):
        self._tiles = list(tiles_snapshot)
        self._blank_count = tiles_snapshot.count('?')
        self._unique_tiles = set(tiles_snapshot) - {'?'}

