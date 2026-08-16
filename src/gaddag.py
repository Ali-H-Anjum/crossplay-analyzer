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
    
class realGADDAGArc:
    __slots__ = ['destination', 'letter_set']

    def __init__(self, destination):
        self.destination = destination
        self.letter_set = set()

    def __str__(self):
        return str(self.destination) + "Letter set: " + str(self.letter_set) + "\n"
    
class realGADDAGNode:
    __slots__ = ['arcs', 'is_terminal']

    def __init__(self):
        self.arcs = {}
        self.is_terminal = False

    def __str__(self):
        return "Next arcs: " + str(self.arcs.keys()) + "\nTerminal: " + str(self.is_terminal) + "\n"

    def get_arc(self, letter):
        return self.arcs.get(letter)

    def get_destination(self, letter):
        arc = self.arcs.get(letter)
        return arc.destination if arc else None

class realGADDAG:
    def __init__(self, path='wordList.txt'):
        self._root = realGADDAGNode()
        self._node_count = 1

        with open(path, 'r') as file:
            for line in file:
                for word in line.strip().split():
                    self.add_word(word.upper())

        print(f"GADDAG has {self._node_count} nodes")

    def get_root(self):
        return self._root

    def next_arc(self, node: realGADDAGNode, letter: str):
        target = node if node is not None else self._root
        arc = target.arcs.get(letter) 
        return arc.destination if arc else None

    def add_word(self, word: str):
        n = len(word)
        a = word

        state = self._root
        for i in range(n - 1, 1, -1):
            state = self._add_arc(state, a[i])
        self._add_final_arc(state, a[1], a[0])

        state = self._root
        for i in range(n - 2, -1, -1):
            state = self._add_arc(state, a[i])
        state = self._add_final_arc(state, '^', a[n - 1])

        first_iteration = True
        for m in range(n - 2, 0, -1):
            force_st = state
            state = self._root
            for i in range(m - 1, -1, -1):
                state = self._add_arc(state, a[i])
            state = self._add_arc(state, '^')
            arc = self._force_arc(state, a[m], force_st)

            if first_iteration:
                arc.letter_set.add(a[n - 1])
                first_iteration = False

    def _add_arc(self, st: realGADDAGNode, ch: str) -> realGADDAGNode:
        if ch not in st.arcs:
            st.arcs[ch] = realGADDAGArc(realGADDAGNode())
            self._node_count += 1
        return st.arcs[ch].destination

    def _add_final_arc(self, st: realGADDAGNode, c1: str, c2: str):
        if c1 not in st.arcs:
            st.arcs[c1] = realGADDAGArc(realGADDAGNode())
            self._node_count += 1
        st.arcs[c1].letter_set.add(c2)
        st.arcs[c1].destination.is_terminal = True
        return st.arcs[c1].destination

    def _force_arc(self, st: realGADDAGNode, ch: str, force_st: realGADDAGNode):
        if ch in st.arcs:
            existing = st.arcs[ch].destination
            if existing is not force_st:
                raise ValueError(f"Conflict: existing destination {existing} is not the forced destination {force_st}")
        else:
            st.arcs[ch] = realGADDAGArc(force_st)
        return st.arcs[ch]  

    def get_letter_set(self, node, letter):
        arc = node.arcs.get(letter)
        return arc.letter_set if arc else set()

    def cross_check(self, prefix, suffix):
        if not prefix and not suffix:
            return set('ABCDEFGHIJKLMNOPQRSTUVWXYZ')

        rev_prefix = prefix[::-1]
        valid = set()

        for L in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            arc = self._root.arcs.get(L)
            if arc is None:
                continue
            node = arc.destination

            ok = True
            for c in rev_prefix:
                arc = node.arcs.get(c)
                if arc is None:
                    ok = False
                    break
                node = arc.destination
            if not ok:
                continue

            if suffix:
                arc = node.arcs.get('^')
                if arc is None:
                    continue
                node = arc.destination
                for c in suffix:
                    arc = node.arcs.get(c)
                    if arc is None:
                        ok = False
                        break
                    node = arc.destination
                if not ok:
                    continue

            if node.is_terminal:
                valid.add(L)

        return valid


def bfs_all_paths(root):
    visited = {id(root)}
    queue = deque([(root, "")])
    paths_by_node = {}

    while queue:
        node, path = queue.popleft()
        paths_by_node.setdefault(id(node), []).append(path or "<root>")

        arcs = node.arcs if isinstance(node.arcs, dict) else \
               {c: a for c, a in zip(_CHARS, node.arcs) if a is not None}

        for letter, arc in arcs.items():
            child = arc.destination
            if id(child) not in visited:
                visited.add(id(child))
                queue.append((child, path + letter))

    return paths_by_node.values()

def verbose_test(root):
    d = 0
    q = [root]
    visited = {id(root)}

    while q:
        d = d + 1
        level_size = len(q)
        level = []

        print("*********************** Level depth: " + str(d) + " ***********************\n")

        for _ in range(level_size):
            node = q.pop(0)
            level.append(node)

            for letter, arc in reversed(node.arcs.items()):

                if id(arc.destination) not in visited:
                    visited.add(id(arc.destination))
                    q.append(arc.destination)

                print(letter)
                print(arc)


# g = GADDAG(path="test.txt")
# print(bfs_all_paths(g.get_root()))

#####################################################

# g1 = realGADDAG(path="test.txt")
# print(bfs_all_paths(g1.get_root()))
# print()

# root1 = g1.get_root()
# verbose_test(root1)

# r2 = g1.next_arc(root1, "A")
# r3 = g1.next_arc(r2, "C")
# r4 = g1.next_arc(r3, "^")
# r5 = g1.next_arc(r4, "R")
# print(g1.next_arc(r5, "^").arcs)

# print(g1.get_letter_set(g1.get_root(), "R"))

# print(root1.arcs.keys())

#####################################################

# g2 = realGADDAG(path="test2.txt")
# print(bfs_all_paths(g2.get_root()))
# print()

# root2 = g2.get_root()
# verbose_test(root2)

#####################################################

# g3 = realGADDAG(path="OSPD.txt")
# g3_root = g3.get_root()
# print(g3_root.get_arc("E").destination.get_arc("R").destination.get_arc("A").destination.get_arc("C").letter_set)

#####################################################


# Ctest = GADDAG("alphabetical_word_list.txt")
# Ctest_root = Ctest.get_root()
# print(Ctest.next_arc(Ctest_root, "A").arcs)

# Btest = realGADDAG("alphabetical_word_list.txt")
# Btest_root = Btest.get_root()
# print(Btest_root.get_arc("A"))









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


