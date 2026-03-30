class GADDAGNode:
    __slots__ = ['next', 'terminal']

    def __init__(self):
        self.next = {}
        self.terminal = False


class GADDAG:
    def __init__(self):
        self.root = GADDAGNode()
        self.word_count = 0

    def add_word(self, word):
        self._add_path(word)

        for i in range(1, len(word)):
            rotated = word[i] + word[:i] + word[i+1:]
            self._add_path(rotated)

    def _add_path(self, path):
        current = self.root

        for char in path:
            if char not in current.next:
                current.next[char] = GADDAGNode()
            current = current.next[char]

        if not current.terminal:
            current.terminal = True
            self.word_count += 1

    def get_all_words(self):
        words = []
        self._dfs_collect(self.root, "", words)
        return words
    
    def _dfs_collect(self, node, current_word, words):
        if node.terminal:
            words.append(current_word)

        for char, next_node in sorted(node.next.items()):
            self._dfs_collect(next_node, current_word + char, words)


####################################
g = GADDAG()

with open('wordList.txt', 'r') as file:
    for line in file:
        for word in line.strip().split():
            g.add_word(word)


print(g.get_all_words(), g.word_count)


