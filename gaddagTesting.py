class GADDAGNode:
    __slots__ = ['next', 'is_terminal']

    def __init__(self):
        self.next = {}
        self.is_terminal = False


class GADDAG:
    def __init__(self):
        self.root = GADDAGNode()
        self.node_count = 1

    def add_word(self, word):
        self._add_path(word)

        length = len(word)

        for i in range(1, length + 1):
            before = word[:i][::-1]
            after = word[i:]
            if i == length: rotated = before
            else: rotated = before + '^' + after
            
            self._add_path(rotated)

    def _add_path(self, path):
        current = self.root

        for char in path:
            if char not in current.next: 
                current.next[char] = GADDAGNode()
                self.node_count += 1
            current = current.next[char]
            
        current.is_terminal = True
            
    def is_valid(self, word):
        node = self.get_last_node_in_path(word)
        return node is not None 

    def get_last_node_in_path(self, path):
        current = self.root

        for char in path:
            if char not in current.next:
                return None
            current = current.next[char]
        return current

    def get_all_words(self):
        words = []
        self._dfs_collect(self.root, "", words)
        return words
    
    def _dfs_collect(self, node, current_word, words):
        if node.terminal:
            words.append(current_word)

        for char, next_node in sorted(node.next.items()):
            self._dfs_collect(next_node, current_word + char, words)


    # def get_moves(self, tiles, board_tiles):
    #     moves = set()

    #     print(tiles, board_tiles)

    #     for i, letter in enumerate(tiles):
    #         tiles_remaining = tiles[:i] + tiles[i+1:]

    #         self.expand_letter(self.root, letter, tiles_remaining, board_tiles, letter, moves)

    # def expand_letter(self, node, current_letter, tiles_remaining, board_tiles, current_word, moves):
    #     if node.is_terminal: moves.add(current_word)

    #     for next_char, next_node in node.next.items():
    #         board_tile_match = False

    #         for tile, x, y in board_tiles:



# ####################################


