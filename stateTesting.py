class DAWGNode:
    def __init__(self):
        self.children = {}
        self.is_terminal = False
        self.count = 0  # number of words in subtree

    def __repr__(self):
        return f"DAWGNode(terminal={self.is_terminal}, children={list(self.children.keys())})"


class DAWG:
    def __init__(self):
        self.root = DAWGNode()
        self._minimized_nodes = {}  # suffix -> node (for minimization)
        self._unchecked_nodes = []  # (parent, char, child) triples
        self._previous_word = ""

    # ------------------------------------------------------------------ #
    #  Core helpers                                                        #
    # ------------------------------------------------------------------ #

    def _node_key(self, node: DAWGNode):
        """Canonical key for a node used during minimization."""
        return (node.is_terminal, tuple(sorted(node.children.items())))

    def _minimize(self, down_to: int):
        """
        Minimize nodes from the end of _unchecked_nodes down to `down_to`.
        Replaces duplicate suffix nodes with a single shared node.
        """
        for i in range(len(self._unchecked_nodes) - 1, down_to - 1, -1):
            parent, char, child = self._unchecked_nodes[i]
            key = self._node_key(child)
            if key in self._minimized_nodes:
                # Replace child with the already-minimized equivalent
                parent.children[char] = self._minimized_nodes[key]
            else:
                self._minimized_nodes[key] = child
        self._unchecked_nodes = self._unchecked_nodes[:down_to]

    # ------------------------------------------------------------------ #
    #  Building (words MUST be inserted in sorted order)                  #
    # ------------------------------------------------------------------ #

    def insert(self, word: str):
        """Insert a word. Words must be added in lexicographic order."""
        if word < self._previous_word:
            raise ValueError("Words must be inserted in sorted (lexicographic) order.")

        # Find the length of the common prefix with the previous word
        common_prefix_len = 0
        for i in range(min(len(word), len(self._previous_word))):
            if word[i] != self._previous_word[i]:
                break
            common_prefix_len += 1
        else:
            common_prefix_len = min(len(word), len(self._previous_word))

        # Minimize nodes that diverge from the new word
        self._minimize(common_prefix_len)

        # Build new nodes for the suffix that differs
        node = (
            self._unchecked_nodes[-1][2]
            if self._unchecked_nodes
            else self.root
        )
        for char in word[common_prefix_len:]:
            print(char)
            new_node = DAWGNode()
            node.children[char] = new_node
            self._unchecked_nodes.append((node, char, new_node))
            node = new_node

        node.is_terminal = True
        self._previous_word = word

    def finish(self):
        """Call after all inserts to finalize minimization."""
        self._minimize(0)

    # ------------------------------------------------------------------ #
    #  Querying                                                            #
    # ------------------------------------------------------------------ #

    def search(self, word: str) -> bool:
        """Return True if `word` is in the DAWG."""
        node = self.root
        for char in word:
            if char not in node.children:
                return False
            node = node.children[char]
        return node.is_terminal

    def starts_with(self, prefix: str) -> bool:
        """Return True if any word in the DAWG starts with `prefix`."""
        node = self.root
        for char in prefix:
            if char not in node.children:
                return False
            node = node.children[char]
        return True

    def words_with_prefix(self, prefix: str) -> list[str]:
        """Return all words that start with `prefix`."""
        node = self.root
        for char in prefix:
            if char not in node.children:
                return []
            node = node.children[char]
        results = []
        self._dfs(node, prefix, results)
        return results

    def _dfs(self, node: DAWGNode, current: str, results: list):
        if node.is_terminal:
            results.append(current)
        for char, child in node.children.items():
            self._dfs(child, current + char, results)

    def all_words(self) -> list[str]:
        """Return every word stored in the DAWG."""
        return self.words_with_prefix("")

    def node_count(self) -> int:
        """Count unique nodes (demonstrates sharing/compression)."""
        visited = set()
        stack = [self.root]
        while stack:
            node = stack.pop()
            if id(node) not in visited:
                visited.add(id(node))
                stack.extend(node.children.values())
        return len(visited)
    





##########################################################################
# testing_word = 'CARE'
# full_paths = []

# length = len(testing_word)

# for i in range(1, length + 1):
#     prefix = testing_word[:i][::-1]
#     suffix = testing_word[i:]

#     if i == length: 
#         rotated = prefix
#     else: 
#         rotated = prefix + '^' + suffix

#     full_paths.append(rotated)

# print(sorted(full_paths))



if __name__ == "__main__":
    words = sorted(['AC^RE', 'C^ARE', 'ERAC', 'RAC^E'])

    dawg = DAWG()
    for word in words:
        dawg.insert(word)
    dawg.finish()

    # Search
    print(dawg.search("apple"))     # True
    print(dawg.search("app"))       # False
    print(dawg.search("bandana"))   # True

    # Prefix check
    print(dawg.starts_with("app"))  # True
    print(dawg.starts_with("xyz"))  # False

    # Words with prefix
    print(dawg.words_with_prefix("ba"))  # ['bat', 'ball', 'ban', 'band', 'bandana']

    # Compression stat
    print(f"Words: {len(words)}, Nodes: {dawg.node_count()}")













