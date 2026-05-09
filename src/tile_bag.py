import random

class Tilebag: #Could be remade as a list of count ints
    def __init__(self):
        self._tile_bag = [
            '?', '?', '?', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 'B', 'C', 'C', 'D', 'D', 'D', 'D', 
            'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E', 'F', 'F', 'G', 'G', 'G', 'H', 'H', 'H',
            'I', 'I', 'I', 'I', 'I', 'I', 'I', 'I', 'J', 'K', 'L', 'L', 'L', 'L', 'M', 'M', 'N', 'N', 'N', 'N',
            'N', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O', 'P', 'P', 'Q', 'R', 'R', 'R', 'R', 'R', 'R', 'S', 'S',
            'S', 'S', 'S', 'T', 'T', 'T', 'T', 'T', 'T', 'U', 'U', 'U', 'V', 'V', 'W', 'W', 'X', 'Y', 'Y', 'Z']
        
        # self._tile_distribution = {'?': 3, 'A': 9, 'B': 2, 'C': 2, 'D': 4, 'E': 12, 'F': 2, 'G': 3, 'H': 3,'I': 8, 'J': 1, 'K': 1, 'L': 4, 'M': 2, 
        #                            'N': 5, 'O': 8, 'P': 2, 'Q': 1,'R': 6, 'S': 5, 'T': 6, 'U': 3, 'V': 2, 'W': 2, 'X': 1, 'Y': 2, 'Z': 1}  
        # self._tilebag = []
        # for tile, count in self._tile_distribution.items():
        #     self._tilebag.extend([tile] * count)
    
    def draw_tiles(self, number_of_tiles: int):
        number_of_tiles = min(number_of_tiles, len(self._tile_bag))
        random_tiles = random.sample(self._tile_bag, number_of_tiles)

        for tile in random_tiles:
            self._tile_bag.remove(tile)

        return random_tiles
    
    def __len__(self):
        return len(self._tile_bag)
    

    ##################### AI METHODS #####################

    def snapshot(self):
        return tuple(self._tile_bag)
    
    def restore(self, tile_bag_snapshot):
        self._tile_bag = list(tile_bag_snapshot)