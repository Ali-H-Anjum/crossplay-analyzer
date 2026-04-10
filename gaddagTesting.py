class GADDAGNode:
    __slots__ = ['next', 'is_terminal']

    def __init__(self):
        self.next = {}
        self.is_terminal = False


class GADDAG:
    def __init__(self, path='wordList.txt'):
        self._root = GADDAGNode()
        self._node_count = 1

        with open(path, 'r') as file:
            for line in file:
                for word in line.strip().split():
                    self.add_word(word)

    def get_root(self):
        return self._root
    
    def get_next(self, node, letter):
        if letter in node.next:
            return node.next[letter]
        return 0
    
    def next_arc(self, arc, letter):
        if arc == 0:
            return self.get_root()
        if letter == 0:
            return self.get_next(arc, '^')
        return self.get_next(arc, letter)
    
    def is_on_arc(self, arc, letter):
        return self.next_arc(arc, letter) != 0

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
        current = self._root

        for char in path:
            if char not in current.next: 
                current.next[char] = GADDAGNode()
                self._node_count += 1
            current = current.next[char]
            
        current.is_terminal = True
    
    def contains_whole_word(self, word):
        if not word: return False
        node = self.get_last_node_in_path(word[::-1] + '^')
        return node is not None and node.is_terminal

    def get_last_node_in_path(self, path, start = None):
        current = self._root if start is None else start

        for char in path:
            if char not in current.next: return None
            current = current.next[char]
        return current
    
    def cross_check(self, prefix, suffix):
        valid_letters = set()

        if prefix:
            path = prefix[::-1] + '^'
            node = self.get_last_node_in_path(path)

            if node is None:
                return valid_letters
        
            for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
                if letter not in node.next:
                    continue

                candidate_node = self.get_last_node_in_path(suffix, start=node.next[letter])
                if candidate_node is not None and candidate_node.is_terminal:
                    valid_letters.add(letter)

        else:
            path = suffix[::-1]
            node = self.get_last_node_in_path(path)
            if node is None:
                return valid_letters
            for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
                if letter not in node.next:
                    continue
                if node.next[letter].is_terminal:
                    valid_letters.add(letter)

        return valid_letters

   

