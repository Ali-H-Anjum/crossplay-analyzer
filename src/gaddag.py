import pickle
import os

_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ^'
_CHAR_INDEX = {c: i for i, c in enumerate(_CHARS)}

class GADDAGArc:
    __slots__ = ['destination', 'letter_set']

    def __init__(self, destination):
        self.destination = destination
        self.letter_set = set()


class GADDAGNode:
    __slots__ = ['arcs', 'is_terminal']

    def __init__(self):
        self.arcs = [None] * 27
        self.is_terminal = False

    def get_destination(self, index):
        arc = self.arcs[index]
        return arc.destination if arc else None 

class GADDAG:
    def __init__(self, path='wordList.txt', cache_path = 'gaddag.pkl'):
        if os.path.exists(cache_path) and False:
            with open(cache_path, 'rb') as f:
                cached = pickle.load(f)
                self._root = cached._root
                self._node_count = cached._node_count
        else:
            self._root = GADDAGNode()
            self._node_count = 1

            with open(path, 'r') as file:
                for line in file:
                    for word in line.strip().split():
                        self.add_word(word)

            # with open(cache_path, 'wb') as f:
            #     pickle.dump(self, f)

            print("The GADDAG has " + str(self._node_count) + " nodes")

    def get_root(self): 
        return self._root
    
    def next_arc(self, arc, letter):
        index = _CHAR_INDEX[letter]
        if arc is not None:
            return arc.get_destination(index)
        return self._root.get_destination(index)

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
                new_node = GADDAGNode()
                current.arcs[index] = GADDAGArc(new_node)
                self._node_count += 1
            current = current.arcs[index].destination
        current.is_terminal = True

    def cross_check(self, prefix, suffix):
        if not prefix and not suffix:
            return set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')
        
        rev_prefix = prefix[::-1]
        valid = set()

        for L in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            arc = self._root.arcs[_CHAR_INDEX[L]]
            if arc is None:
                continue
            node = arc.destination

            ok = True
            for c in rev_prefix:
                arc = node.arcs[_CHAR_INDEX[c]]
                if arc is None:
                    ok = False
                    break
                node = arc.destination
            if not ok:
                continue

            if suffix:
                arc = node.arcs[_CHAR_INDEX['^']]
                if arc is None:
                    continue
                node = arc.destination
                for c in suffix:
                    arc = node.arcs[_CHAR_INDEX[c]]
                    if arc is None:
                        ok = False
                        break
                    node = arc.destination
                if not ok:
                    continue

            if node.is_terminal:
                valid.add(L)

        return valid