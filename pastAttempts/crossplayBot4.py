from collections import Counter
from operator import itemgetter

def create_board():
    board = [[' ' for _ in range(15)] for _ in range(15)]
    return board

def show_board(b):
    for i in range(15):
        print(b[i])

def add_letter_to_board(board, letter, x, y):
    board[14-y][x] = letter

    if((x, y) in double_letter_multipliers):
        double_letter_multipliers.remove((x, y))
    elif((x, y) in triple_letter_multipliers):
        triple_letter_multipliers.remove((x, y))
    elif((x, y) in double_word_multipliers):
        double_word_multipliers.remove((x, y))
    elif((x, y) in triple_word_multipliers):
        triple_word_multipliers.remove((x, y))


def get_points_per_letter(letter, x, y):
    m = 1
    if((x,y) in double_letter_multipliers):
        m = m + 1
        
    elif((x, y) in triple_letter_multipliers):
        m = m + 2
        
    return points_per_tile.get(letter, 0) * m

def get_points_per_move(word, x, y, direction):
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

            points += get_points_per_letter(i.upper(), x, y)

            if (direction == 'd'):
                y = y - 1
            elif(direction == 'a'):
                x = x + 1
            
        return points * n
    
def add_word_to_board(board, word, x, y, direction):
    if(not in_bounds(x, y) or (direction == 'd' and not in_bounds(x, y - len(word) + 1) or direction == 'a' and not in_bounds(x + len(word) - 1, y))):
        print("error")
    else:
        for i in word:
            add_letter_to_board(board, i, x, y)
            if (direction == 'd'):
                y = y - 1
            elif(direction == 'a'):
                x = x + 1
                
    
def in_bounds(x, y):
    if((x < 0) or (x > 14) or (y < 0) or (y > 14)):
        return False
    else:
        return True
    
def get_all_initial_positions(word): #Assuming it is at most 7 letters long
    points_per_position = {} #Might look into using heap instead
    length = len(word)
    for i in range(length):
        x = 8 + i - length 
        points_per_position.setdefault((x, 7, 'a'), []).append(get_points_per_move(word, x, 7, 'a'))
        y = 7 + i
        points_per_position.setdefault((7, y, 'd'), []).append(get_points_per_move(word, 7, y, 'd'))

    return points_per_position

def get_most_points_in_dictionary(dict): #Given via AI
    return max(dict.items(), key=lambda item: item[1])

def create_word_dictionary(): #turns my file into a dictionary of signatures (sorted words) : words. (Reduces search by 20k lines)
    word_dictionary = {}
    with open("NWL2023-Playability.txt", 'r') as file:
        for line in file:
            parts = line.strip().split()
            
            if len(parts) >= 2:
                word = parts[1].lower()
                signature = ''.join(sorted(word))

                if signature in word_dictionary:
                    word_dictionary[signature].append(word)
                else:
                    word_dictionary[signature] = [word]

    return word_dictionary

def get_all_initial_words(word_dictionary, tiles):
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

def get_best_initial_moves(word_dictionary, tiles): #Uses forward pruning
    best_initial_moves = {}

    all_initial_words = get_all_initial_words(word_dictionary, tiles)

    for word in all_initial_words:
        all_initial_positions = get_all_initial_positions(word)
        position_for_most_points, most_points = get_most_points_in_dictionary(all_initial_positions) #Forward pruning example
        x, y, direction = position_for_most_points
        best_initial_moves[word, x, y, direction] = most_points

    return fast_sort(best_initial_moves)

def get_all_initial_moves(word_dictionary, tiles): #Does not use forward pruning
    all_initial_moves = {}

    all_initial_words = get_all_initial_words(word_dictionary, tiles)

    for word in all_initial_words:
        all_initial_positions = get_all_initial_positions(word)

        for key, value in all_initial_positions.items():
            x, y, direction = key
            all_initial_moves[word, x, y, direction] = value

    return fast_sort(all_initial_moves)
        
def fast_sort(dictionary): #Given via AI
    return dict(sorted(dictionary.items(), key = itemgetter(1), reverse = True))

def get_all_words_improved(word_dictionary, tiles, played_moves):
    all_words = set()
    tiles_lower = [t.lower() for t in tiles]

    for played_move in played_moves:
        word, x, y, direction = played_move

        same_direction_words = get_moves_in_same_direction(word_dictionary, word, tiles_lower)
        different_direction_words = get_moves_in_different_direction(word_dictionary, word, tiles_lower)

        all_words.update(same_direction_words)
        all_words.update(different_direction_words)

        return sorted(all_words)

def get_moves_in_same_direction(word_dictionary, word, tiles_lower):
    same_direction_words = set()

    available_tile_counts = Counter(tiles_lower)

    for letter in word:
        available_tile_counts[letter] += 1

    for signature, potential_expansions in word_dictionary.items():
        if(len(signature) < len(word)):
            continue

        if(len(signature) > available_tile_counts.total()):
            continue

        skip = False
        for potential_expansion in potential_expansions:
            if word not in potential_expansion or word == potential_expansion:
                skip = True
                break

        if skip:
            continue

        if (is_valid_expansion(signature, available_tile_counts)):
            same_direction_words.update(potential_expansions)

    return same_direction_words

def get_moves_in_different_direction(word_dictionary, word, tiles_lower):
    different_direction_words = set()

    for letter in word:
        available_tile_counts = Counter(tiles_lower)
        available_tile_counts[letter] += 1

        for signature, potential_expansions in word_dictionary.items():
            if(letter not in signature):
                continue
        
            if(len(signature) > available_tile_counts.total()):
                continue

            if(is_valid_expansion(signature, available_tile_counts)):
                different_direction_words.update(potential_expansions)

    return different_direction_words

def is_valid_expansion(signature, available_tile_counts):
    signature_letter_counts = Counter(signature)
    blank_count = available_tile_counts.get('?', 0)
    blanks_needed = 0
    possible = True

    for signature_letter, signature_letter_count in signature_letter_counts.items():
        available_tile_count = available_tile_counts.get(signature_letter, 0)
        
        if signature_letter_count > available_tile_count:
            blanks_needed += signature_letter_count - available_tile_count

            if blanks_needed > blank_count:
                possible = False
                break

    return possible

def simulated_game_test():
    board = create_board()

    player1_tiles = ['E', 'R', 'J', 'C', 'H', 'E', 'A']
    player2_tiles = ['A', 'L', 'I', 'G', 'D', 'H', 'I']

    word_dictionary = create_word_dictionary()

    played_moves_to_points = {}

    all_initial_moves = get_all_initial_moves(word_dictionary, player1_tiles)

    move_for_most_points, most_points = get_most_points_in_dictionary(all_initial_moves)

    played_moves_to_points[move_for_most_points] = most_points
    
    word, x, y, direction = move_for_most_points

    print(f"{played_moves_to_points}")

    add_word_to_board(board, word, x, y, direction)

    show_board(board)

    all_moves = get_all_words_improved(word_dictionary, player2_tiles, played_moves_to_points.keys())

    print(all_moves)

    

    



    
points_per_tile = {'?': 0, 'A': 1, 'B': 4, 'C': 3, 'D': 2, 'E': 1, 'F': 4, 'G': 4, 'H': 3,'I': 1, 'J': 10, 'K': 6, 'L': 2, 'M': 3,
                    'N': 1, 'O': 1, 'P': 3, 'Q': 10,'R': 1, 'S': 1, 'T': 1, 'U': 2, 'V': 6, 'W': 5, 'X': 8, 'Y': 4, 'Z': 10}
    
double_letter_multipliers = {(9,7),(7,9),(5,7),(7,5),(14,7),(7,14),(0,7),(7,0),(12,10),(11,11),(10,12),(4,12),(3,11),(2,10),(2,4),(3,3),(4,2),(10,2),(11,3),(12,4)}

triple_letter_multipliers = {(10,9),(9,10),(5,10),(4,9),(4,5),(5,4),(9,4),(10,5),(13,6),(13,8),(8,13),(6,13),(1,8),(1,6),(6,1),(8,1),(14,0),(14,14),(0,14),(0,0)}

double_word_multipliers = {(11,7),(7,11),(3,7),(7,3),(13,1),(13,13),(1,13),(1,1)}

triple_word_multipliers = {(14,11),(11,14),(3,14),(0,11),(0,3),(3,0),(11,0),(14,3)}

#example_tiles = ['E', 'R', 'J', 'C', 'H', 'E', 'A']
    
#board = create_board()

#show_board(board)
    
#print(add_word(board, "help", 14, 3, 'd'))

#all_positions = find_all_positions_for_first_move("help")

#print(all_positions)

#print(most_points(all_positions))

#wd = create_word_dictionary()

#print(best_first_move(wd, example_tiles))

#print(get_move_with_max_points(get_best_initial_move(wd, example_tiles)))

simulated_game_test()