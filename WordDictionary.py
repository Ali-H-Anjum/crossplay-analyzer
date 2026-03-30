from collections import defaultdict

class WordDictionary:
    def __init__(self):
        self.__all_words = set()
        self.__word_dictionary = defaultdict(set)
        self.__all_substrings = set()

        with open("NWL2023-Playability.txt", 'r') as file:
            for line in file:
                parts = line.strip().split()

                if len(parts) >= 2:
                    word = parts[1].lower()
                    self.__all_words.add(word)

                    word_length = len(word)
                    for i in range(word_length):
                        for j in range(i + 2, word_length + 1):
                            self.__all_substrings.add(word[i:j])

                    signature = ''.join(sorted(word))
                    self.__word_dictionary[signature].add(word)

        print(len(self.__all_words), len(self.__word_dictionary), len(self.__all_substrings))
                        
    def get_word_dictionary(self): return self.__word_dictionary
    
    def get_all_words(self): return self.__all_words

    def get_all_substrings(self): return self.__all_substrings
    
    def valid_word(self, word):
        word_length = len(word)
        if word_length < 2 or word_length > 15: return False

        return word in self.__all_words
    
    def partial_word(self, word): #WILL REPLACE WITH GADDAG
        word_length = len(word)
        if word_length < 2 or word_length > 15: return False

        return word in self.__all_substrings




