import pickle
import os
import cProfile

from collections import deque

from letter_mask import *

_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ^'


class Arc:
    __slots__ = ['destination_state', 'letter_set']

    def __init__(self, destination_state: State):
        self.destination_state = destination_state
        self.letter_set = 0 #Start with empty set of letters

    # def __str__(self):
    #     return str(self.destination_state) + "Letter set: " + str(self.letter_set) + "\n"

    def next_arc(self, letter):
        return self.destination_state.arcs.get(letter) 
    
class State:
    __slots__ = ['arcs']

    def __init__(self):
        self.arcs = {}

    # def __str__(self):
    #     return "Next arcs: " + str(self.arcs.keys()) + "\n"

class Gaddag:
    def __init__(self, path = 'wordList.txt', cache_path = 'gaddag.pkl'):
        if os.path.exists(cache_path) and False:
            with open(cache_path, 'rb') as f:
                cached = pickle.load(f)
                self.root = cached.root
                self.init = cached.init
                self.size = cached.size


        else:
            self.root = State()
            self.init = Arc(self.root)
            self.init.letter_set = None

            self.size = 1

            with open(path, 'r') as file:
                for line in file:
                    for word in line.strip().split():
                        self.add_word(word.upper())

            # with open(cache_path, 'wb') as f:
            #     pickle.dump(self, f)

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
            st.arcs[c1].letter_set = add_char(st.arcs[c1].letter_set, c2)
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
                arc.letter_set = add_char(arc.letter_set, a[n - 1])
                first_iteration = False

    def get_init(self):
        return self.init

   
                          

