class Tiles:
    def __init__(self, tiles):
        self._tiles = tiles
        self._blank_count = tiles.count('?')
        self._unique_tiles = set(tiles) - {'?'}

    def remove_tile(self, tile):
        self._tiles.remove(tile)
        if tile == '?':
            self._blank_count -= 1
        else:
            if tile not in self._tiles:
                self._unique_tiles.remove(tile)

    def restore_tile(self, tile):
        self._tiles.append(tile)  # order doesn't matter for generation
        if tile == '?':
            self._blank_count += 1
        else:
            self._unique_tiles.add(tile)

    def add_tiles(self, tiles):
        self._tiles.extend(tiles)
        self._blank_count = self._tiles.count('?')
        self._unique_tiles = set(self._tiles) - {'?'}

    def play_tile(self, tile):
        self._tiles.remove(tile)
        self._blank_count = self._tiles.count('?')
        self._unique_tiles = set(self._tiles) - {'?'}

    def get_tiles(self): #Immutable
        return tuple(self._tiles) 

    def get_unique_tiles(self):
        return self._unique_tiles
    
    def get_blank_count(self):
        return self._blank_count
    
    def __str__(self):
        return  "Tiles: " + "".join([str(item) for item in self._tiles])

    def __len__(self):
        return len(self._tiles)
    
    def __bool__(self):
        return bool(self._tiles)