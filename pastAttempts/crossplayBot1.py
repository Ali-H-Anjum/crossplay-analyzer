from collections import Counter

def create_board():
    board = [[' ' for _ in range(15)] for _ in range(15)]
    return board

def show_board(b):
    for i in range(15):
        print(b[i])

def add_letter(b, letter, x, y):
    b[14-y][x] = letter

def add_letter_points(letter, x, y):
    m = 1
    if((x,y) in double_letter_multipliers):
        m = m + 1
        double_letter_multipliers.remove((x, y))
    elif((x, y) in triple_letter_multipliers):
        m = m + 2

    return points_per_tile.get(letter, 0) * m

def calculate_word_points(word, direction, x, y):
    if(not in_bounds(x, y) or (direction == 'd' and not in_bounds(x, y - len(word) + 1) or direction == 'a' and not in_bounds(x + len(word) - 1, y))):
        print("error")
    else:
        n = 1
        points = 0
        for i in word:
            if((x, y) in double_word_multipliers):
                n *= 2
            elif((x, y) in triple_word_multipliers):
                n *= 3

            points += add_letter_points(i.upper(), x, y)

            if (direction == 'd'):
                y = y - 1
            elif(direction == 'a'):
                x = x + 1
            
        return points * n
    
def add_word(b, word, direction, x, y):
    if(not in_bounds(x, y) or (direction == 'd' and not in_bounds(x, y - len(word) + 1) or direction == 'a' and not in_bounds(x + len(word) - 1, y))):
        print("error")
    else:
        for i in word:
            add_letter(b, i, x, y)
            if (direction == 'd'):
                y = y - 1
            elif(direction == 'a'):
                x = x + 1
                
    
def in_bounds(x, y):
    if((x < 0) or (x > 14) or (y < 0) or (y > 14)):
        return False
    else:
        return True
    
def find_all_positions_for_first_move(word): #Assuming it is at most 7 letters long
    points_per_position = {} #Might look into using heap instead
    length = len(word)
    for i in range(length):
        x = 8 + i - length 
        points_per_position.setdefault((x, 7, 'a'), []).append(calculate_word_points(word, 'a', x, 7))
        y = 7 + i
        points_per_position.setdefault((7, y, 'd'), []).append(calculate_word_points(word, 'd', 7, y))

    return points_per_position

def most_points(dict):
    return max(dict, key=lambda k: max(dict[k]))

def create_word_dictionary():
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

    return word_dictionary

def find_all_possible_words(wd, tiles):
    #Preprocess tiles
    tiles_lower = [t.lower() for t in tiles]
    available = Counter(tiles_lower)
    blank_count = available.get('?', 0)

    results = []

    #Loop through every signature 
    for signature, words in wd.items():
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

def best_first_move(wd, tiles):
    best_position_per_word = {}

    all_words = find_all_possible_words(wd, tiles)

    for word in all_words:
        all_positions = find_all_positions_for_first_move(word)
        x, y, direction = most_points(all_positions)
        best_position_per_word.setdefault((word, x, y, direction), []).append(calculate_word_points(word, direction, x, y))

    return best_position_per_word

def simulated_game_test():
    b = create_board

    
points_per_tile = {'?': 0, 'A': 1, 'B': 4, 'C': 3, 'D': 2, 'E': 1, 'F': 4, 'G': 4, 'H': 3,'I': 1, 'J': 10, 'K': 6, 'L': 2, 'M': 3,
                    'N': 1, 'O': 1, 'P': 3, 'Q': 10,'R': 1, 'S': 1, 'T': 1, 'U': 2, 'V': 6, 'W': 5, 'X': 8, 'Y': 4, 'Z': 10}
    
double_letter_multipliers = {(9,7),(7,9),(5,7),(7,5),(14,7),(7,14),(0,7),(7,0),(12,10),(11,11),(10,12),(4,12),(3,11),(2,10),(2,4),(3,3),(4,2),(10,2),(11,3),(12,4)}

triple_letter_multipliers = {(10,9),(9,10),(5,10),(4,9),(4,5),(5,4),(9,4),(10,5),(13,6),(13,8),(8,13),(6,13),(1,8),(1,6),(6,1),(8,1),(14,0),(14,14),(0,14),(0,0)}

double_word_multipliers = {(11,7),(7,11),(3,7),(7,3),(13,1),(13,13),(1,13),(1,1)}

triple_word_multipliers = {(14,11),(11,14),(3,14),(0,11),(0,3),(3,0),(11,0),(14,3)}

example_tiles = ['E', 'R', 'J', 'C', 'H', 'E', 'A']
    
#board = create_board()

#show_board(board)
    
#print(add_word(board, "help", 'd', 14, 3))

#all_positions = find_all_positions_for_first_move("help")

#print(all_positions)

#print(most_points(all_positions))

wd = create_word_dictionary()

print(best_first_move(wd, example_tiles))

print(most_points(best_first_move(wd, example_tiles)))



