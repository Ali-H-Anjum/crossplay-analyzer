# import pickle

_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ^'
_CHAR_INDEX = {c: i for i, c in enumerate(_CHARS)}


class GADDAGNode:
    __slots__ = ['arcs', 'is_terminal']

    def __init__(self):
        self.arcs = [None] * 27
        self.is_terminal = False


class GADDAG:
    def __init__(self, path='wordList.txt'):
        self._root = GADDAGNode()
        self._node_count = 1
        self._word_set = set()

        with open(path, 'r') as file:
            for line in file:
                for word in line.strip().split():
                    self.add_word(word)

                    self._word_set.add(word)

    def in_gaddag(self, word):
        return word in self._word_set

    def get_root(self): return self._root
    
    def get_next(self, node, letter): return node.arcs[_CHAR_INDEX[letter]]
    
    def next_arc(self, arc, letter):
        index = _CHAR_INDEX[letter]
        return arc.arcs[index] if arc is not None else self._root.arcs[index]
    
    def is_on_arc(self, arc, letter): return self.next_arc(arc, letter) is not None

    def add_word(self, word):

        length = len(word)

        for i in range(1, length + 1):
            prefix = word[:i][::-1]
            suffix = word[i:]
            if i == length: rotated = prefix
            else: rotated = prefix + '^' + suffix
            
            self._add_path(rotated)

    def _add_path(self, path):
        current = self._root

        for char in path:
            index = _CHAR_INDEX[char]
            if current.arcs[index] is None:
                current.arcs[index] = GADDAGNode()
                self._node_count += 1
            current = current.arcs[index]
            
        current.is_terminal = True
    
    def contains_whole_word(self, word):
        if not word: return False
        node = self.get_last_node_in_path(word[::-1] + '^')
        return node is not None and node.is_terminal

    def get_last_node_in_path(self, path, start = None):
        current = self._root if start is None else start

        for char in path:
            index = _CHAR_INDEX[char]
            current = current.arcs[index]
            if current is None: return None

        return current

    
    def cross_check(self, prefix, suffix):
        valid_letters = set()

        for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            word = prefix + letter + suffix
            if self.in_gaddag(word):
                valid_letters.add(letter)

        return valid_letters

#######################################

# gaddag = GADDAG()
# with open('gaddag.pkl', 'wb') as f:
#     pickle.dump(gaddag, f)

