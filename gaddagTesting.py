class GADDAGNode:
    __slots__ = ['next', 'is_terminal']

    def __init__(self):
        self.next = {}
        self.is_terminal = False


class GADDAG:
    def __init__(self):
        self.root = GADDAGNode()
        self.word_count = 0

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
            if char not in current.next: current.next[char] = GADDAGNode()
            current = current.next[char]

        if not current.is_terminal:
            current.is_terminal = True
            self.word_count += 1

    def is_valid(self, word):
        current = self.root

        for char in word:
            if char in current.next: current = current.next[char]
            else: return False
            
        if current.is_terminal: return True
        else: return False

    def get_all_words(self):
        words = []
        self._dfs_collect(self.root, "", words)
        return words
    
    def _dfs_collect(self, node, current_word, words):
        if node.terminal:
            words.append(current_word)

        for char, next_node in sorted(node.next.items()):
            self._dfs_collect(next_node, current_word + char, words)


# ####################################
# g = GADDAG()

# g.add_word("explain")

# # with open('wordList.txt', 'r') as file:
# #     for line in file:
# #         for word in line.strip().split():
# #             g.add_word(word)

# print(g.is_valid("TEST"))

# print(g.is_valid("TESTILP"))

