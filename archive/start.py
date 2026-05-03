import random
from collections import Counter

tile_distribution = {
    '?': 3, 
    'A': 9, 
    'B': 2, 
    'C': 2, 
    'D': 4, 
    'E': 12, 
    'F': 2, 
    'G': 3, 
    'H': 3,
    'I': 8, 
    'J': 1, 
    'K': 1, 
    'L': 4, 
    'M': 2, 
    'N': 5, 
    'O': 8, 
    'P': 2, 
    'Q': 1,
    'R': 6, 
    'S': 5, 
    'T': 6, 
    'U': 3, 
    'V': 2, 
    'W': 2, 
    'X': 1, 
    'Y': 2, 
    'Z': 1
    }  

points_per_tile = {
    '?': 0, 
    'A': 1, 
    'B': 4, 
    'C': 3, 
    'D': 2, 
    'E': 1, 
    'F': 4, 
    'G': 4, 
    'H': 3,
    'I': 1, 
    'J': 10, 
    'K': 6, 
    'L': 2, 
    'M': 3, 
    'N': 1, 
    'O': 1, 
    'P': 3, 
    'Q': 10,
    'R': 1, 
    'S': 1, 
    'T': 1, 
    'U': 2, 
    'V': 6, 
    'W': 5, 
    'X': 8, 
    'Y': 4, 
    'Z': 10
    }

tile_bag = []
for tile, count in tile_distribution.items():
    tile_bag.extend([tile] * count)

player1_tiles = random.sample(tile_bag, 7)

for tile in player1_tiles:
    tile_bag.remove(tile)

player2_tiles = random.sample(tile_bag, 7)

for tile in player2_tiles:
    tile_bag.remove(tile)

print("Player 1 has the following tiles:", end=" ")
print(player1_tiles)
print("Player 2 has the following tiles:", end=" ")
print(player2_tiles)

word_dictionary = {}

with open("NWL2023-Playability.txt", 'r') as f:
    for line in f:
        parts = line.strip().split()

        if len(parts) >= 2:
            word = parts[1].lower()
            signature = ''.join(sorted(word))

            if signature in word_dictionary:
                word_dictionary[signature].append(word)
            else:
                word_dictionary[signature] = [word]

def find_words(tiles):
    #Preprocess tiles
    tiles_lower = [t.lower() for t in tiles]
    available = Counter(tiles_lower)
    blank_count = available.get('?', 0)

    results = []

    #Loop through every signature 
    for signature, words in word_dictionary.items():
        word_length = len(signature)
        if word_length > len(tiles):
            continue

        word_counts = Counter(signature)
        blanks_needed = 0
        possible = True

        #Check if the word can be formed with the available tiles and blanks
        for letter, count in word_counts.items():
            available_count = available.get(letter, 0)
            if count > available_count:
                blanks_needed += count - available_count
                if blanks_needed > blank_count:
                    possible = False
                    break

        #If the word is possible, add it to the results
        if possible:
            results.extend(words)
            
            
    results = list(set(results))
    results.sort(key=lambda x: (-len(x), x))

    return results

player1_words = find_words(player1_tiles)
player2_words = find_words(player2_tiles)

def calculate_word_points(words):
    word_points = []

    for word in words:
        points = 0
        for letter in word.upper():
            points += points_per_tile.get(letter, 0)

        word_points.append((word, points))

    word_points.sort(key=lambda x: (-x[1], -len(x[0]), x[0]))

    return word_points

player1_word_points = calculate_word_points(player1_words)
player2_word_points = calculate_word_points(player2_words)

print(player1_words)
print(player2_word_points)








        









