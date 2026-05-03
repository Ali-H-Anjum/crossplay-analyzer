import random

class Tilebag: #Could be remade as a list of count ints
    def __init__(self):
        self._tile_distribution = {'?': 3, 'A': 9, 'B': 2, 'C': 2, 'D': 4, 'E': 12, 'F': 2, 'G': 3, 'H': 3,'I': 8, 'J': 1, 'K': 1, 'L': 4, 'M': 2, 
                                   'N': 5, 'O': 8, 'P': 2, 'Q': 1,'R': 6, 'S': 5, 'T': 6, 'U': 3, 'V': 2, 'W': 2, 'X': 1, 'Y': 2, 'Z': 1}  
        self._tilebag = []
        for tile, count in self._tile_distribution.items():
            self._tilebag.extend([tile] * count)

    def get_tilebag(self):
        return tuple(self._tilebag)
    
    def draw_tiles(self, number_of_tiles):
        number_of_tiles = min(number_of_tiles, len(self._tilebag))
        random_tiles = random.sample(self._tilebag, number_of_tiles)

        for tile in random_tiles:
            self._tilebag.remove(tile)

        return random_tiles
    
    def __len__(self):
        return len(self._tilebag)