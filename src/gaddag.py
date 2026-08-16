import pickle
import os

from collections import deque

_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ^'
_CHAR_INDEX = {c: i for i, c in enumerate(_CHARS)}

class GADDAGArc:
    __slots__ = ['destination', 'letter_set']

    def __init__(self, destination):
        self.destination = destination
        self.letter_set = set()

    def __str__(self):
        return str(self.destination) + "Letter set: " + str(self.letter_set) + "\n"


class GADDAGNode:
    __slots__ = ['arcs', 'is_terminal']

    def __init__(self):
        self.arcs = [None] * 27
        self.is_terminal = False

    def __str__(self):
        return "Next arcs: " + str(self.arcs) + "\nTerminal: " + str(self.is_terminal) + "\n"

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



#############################################################################

class Arc:
    __slots__ = ['destination_state', 'letter_set']

    def __init__(self, destination_state):
        self.destination_state = destination_state
        self.letter_set = set()

    def __str__(self):
        return str(self.destination_state) + "Letter set: " + str(self.letter_set) + "\n"

    def next_arc(self, letter):
        return self.destination_state.arcs.get(letter) 
    
class State:
    __slots__ = ['arcs']

    def __init__(self):
        self.arcs = {}

    def __str__(self):
        return "Next arcs: " + str(self.arcs.keys()) + "\n"

class Gaddag:
    def __init__(self, path = 'wordList.txt'):
        self.root = State()
        self.init = Arc(self.root)
        self.init.letter_set = None

        self.size = 1

        with open(path, 'r') as file:
            for line in file:
                for word in line.strip().split():
                    self.add_word(word.upper())

        print(f"Gaddag has {self.size} nodes")

    def add_word(self, word: str):

        def _add_arc(st: State, ch: str) -> State:
            if ch not in st.arcs:
                st.arcs[ch] = Arc(State())
                self.size += 1
            return st.arcs[ch].destination_state

        def _add_final_arc(st: State, c1: str, c2: str):
            if c1 not in st.arcs:
                st.arcs[c1] = Arc(State())
                self.size += 1
            st.arcs[c1].letter_set.add(c2)
            return st.arcs[c1].destination_state

        def _force_arc(st: State, ch: str, force_st: State):
            if ch in st.arcs:
                existing = st.arcs[ch].destination_state
                if existing is not force_st:
                    raise ValueError(f"Conflict: existing destination {existing} is not the forced destination {force_st}")
            else:
                st.arcs[ch] = Arc(force_st)
            return st.arcs[ch]
                
        n = len(word)
        a = word

        current_state = self.root
        for i in range(n - 1, 1, -1):
            current_state = _add_arc(current_state, a[i])
        _add_final_arc(current_state, a[1], a[0])

        current_state = self.root
        for i in range(n - 2, -1, -1):
            current_state = _add_arc(current_state, a[i])
        current_state = _add_final_arc(current_state, '^', a[n - 1])

        first_iteration = True
        for m in range(n - 2, 0, -1):
            force_st = current_state
            current_state = self.root
            for i in range(m - 1, -1, -1):
                current_state = _add_arc(current_state, a[i])
            current_state = _add_arc(current_state, '^')
            arc = _force_arc(current_state, a[m], force_st)

            if first_iteration:
                arc.letter_set.add(a[n - 1])
                first_iteration = False

    def get_init(self):
        return self.init

    def cross_check(self, prefix, suffix):
        if not prefix and not suffix:
            return set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')

        if not suffix:
            arc = self.init
            for ch in reversed(prefix):
                arc = arc.next_arc(ch)
                if arc is None:
                    return set()
            arc = arc.next_arc('^')
            if arc is None:
                return set()
            return arc.letter_set

        if not prefix:
            arc = self.init
            for ch in reversed(suffix):
                arc = arc.next_arc(ch)
                if arc is None:
                    return set()
            return arc.letter_set

        arc = self.init
        for ch in reversed(prefix):
            arc = arc.next_arc(ch)
            if arc is None:
                return set()
        arc = arc.next_arc('^')
        if arc is None:
            return set()

        valid_letters = set()

        for letter in arc.letter_set:
            current_arc = arc.next_arc(letter)
            if current_arc is None:
                continue
            for ch in suffix:
                if ch not in current_arc.letter_set:
                    break
                current_arc = current_arc.next_arc(ch)
            else:
                valid_letters.add(letter)

        return valid_letters        
                

                

############################################################3

# g = GADDAG(path="alphabetical_word_list.txt")
# print(g.cross_check("AG", "S"))

# t = Gaddag(path="alphabetical_word_list.txt")
# print(t.cross_check("AG", "S"))


