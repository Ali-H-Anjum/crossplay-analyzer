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

def get_points_per_move(word, x, y, is_descending):
    if(not in_bounds(x, y) or (is_descending and not in_bounds(x, y - len(word) + 1) or (not is_descending) and not in_bounds(x + len(word) - 1, y))):
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

            if (is_descending):
                y = y - 1
            elif(not is_descending):
                x = x + 1
            
        return points * n
    
def add_word_to_board(board, word, x, y, is_descending):
    if(not in_bounds(x, y) or (is_descending and not in_bounds(x, y - len(word) + 1) or (not is_descending) and not in_bounds(x + len(word) - 1, y))):
        print("error")
    else:
        for i in word:
            add_letter_to_board(board, i, x, y)
            if (is_descending):
                y = y - 1
            elif(not is_descending):
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
        points_per_position.setdefault((x, 7, False), []).append(get_points_per_move(word, x, 7, False))
        y = 7 + i
        points_per_position.setdefault((7, y, True), []).append(get_points_per_move(word, 7, y, True))

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
        x, y, is_descending = position_for_most_points
        best_initial_moves[word, x, y, is_descending] = most_points

    return fast_sort(best_initial_moves)

def get_all_initial_moves(word_dictionary, tiles): #Does not use forward pruning
    all_initial_moves = {}

    all_initial_words = get_all_initial_words(word_dictionary, tiles)

    for word in all_initial_words:
        all_initial_positions = get_all_initial_positions(word)

        for key, value in all_initial_positions.items():
            x, y, is_descending = key
            all_initial_moves[word, x, y, is_descending] = value

    return fast_sort(all_initial_moves)
        
def fast_sort(dictionary): #Given via AI
    return dict(sorted(dictionary.items(), key = itemgetter(1), reverse = True))

def get_all_words_and_moves(word_dictionary, tiles, played_moves):
    all_words = set()
    all_moves = {}
    tiles_lower = [t.lower() for t in tiles]

    for played_move in played_moves:
        word, x, y, is_descending = played_move

        same_direction_words = get_words_in_same_direction(word_dictionary, word, tiles_lower)
        same_direction_moves = get_moves_in_same_direction(same_direction_words, word, x, y, is_descending)

        different_direction_words = get_words_in_different_direction(word_dictionary, word, tiles_lower)
        different_direction_moves = get_moves_in_different_direction(different_direction_words, word, x, y, is_descending, tiles_lower)

        all_words.update(same_direction_words)
        all_words.update(different_direction_words)

        all_moves.update(same_direction_moves)
        all_moves.update(different_direction_moves)

        return sorted(all_words), all_moves

def get_words_in_same_direction(word_dictionary, word, tiles_lower):
    same_direction_words = set()

    available_tile_counts = Counter(tiles_lower)

    for letter in word: # Might be ideal to have the for loop relate to x, y instead
        available_tile_counts[letter] += 1

    for signature, potential_expansions in word_dictionary.items():
        if(len(signature) < len(word)):
            continue

        if(len(signature) > available_tile_counts.total()):
            continue

        skip = False
        for potential_expansion in potential_expansions: #Might be better to go through every expansion instead of every signature, but this is faster for now
            if word not in potential_expansion or word == potential_expansion:
                skip = True
                break

        if skip:
            continue

        if (is_valid_expansion(signature, available_tile_counts)):
            same_direction_words.update(potential_expansions)

    return same_direction_words

def get_moves_in_same_direction(potential_expansions, word, x, y, is_descending):
    all_moves_in_same_direction = {}

    for potential_expansion in potential_expansions:
        expansion_difference = potential_expansion.find(word)

        if (expansion_difference == -1):
                continue

        if(is_descending):
            potential_expansion_x = x
            potential_expansion_y = expansion_difference + y
        else:
            potential_expansion_x = x - expansion_difference
            potential_expansion_y = y

        all_moves_in_same_direction[potential_expansion, potential_expansion_x, potential_expansion_y, is_descending] = get_points_per_move(potential_expansion, potential_expansion_x, potential_expansion_y, is_descending)

    return all_moves_in_same_direction

def get_words_in_different_direction(word_dictionary, word, tiles_lower):
    different_direction_words = set()

    for letter in word:
        available_tile_counts = Counter(tiles_lower)
        available_tile_counts[letter] += 1

        for signature, potential_expansions in word_dictionary.items():
            if (letter not in signature):
                continue
        
            if (len(signature) > available_tile_counts.total()):
                continue

            if (is_valid_expansion(signature, available_tile_counts)):
                different_direction_words.update(potential_expansions)                

    return different_direction_words

def get_moves_in_different_direction(potential_expansions, word, x, y, is_descending, tiles_lower):
    all_moves_in_different_direction = {}
    
    for potential_expansion in potential_expansions:
        for i, letter in enumerate(word):
            expansion_difference = potential_expansion.find(letter)
            if(expansion_difference > -1): 
                if(is_descending):
                    potential_expansion_x = x - expansion_difference
                    potential_expansion_y = y - i
                else:
                    potential_expansion_x = x + i
                    potential_expansion_y = y + expansion_difference

                all_moves_in_different_direction[potential_expansion, potential_expansion_x, potential_expansion_y, not is_descending] = get_points_per_move(potential_expansion, potential_expansion_x, potential_expansion_y, not is_descending)

    return all_moves_in_different_direction
     
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

    move_for_most_initial_points, most_initial_points = get_most_points_in_dictionary(all_initial_moves)

    played_moves_to_points[move_for_most_initial_points] = most_initial_points
    
    word, x, y, is_descending = move_for_most_initial_points

    print(f"{played_moves_to_points}")

    add_word_to_board(board, word, x, y, is_descending)

    show_board(board)

    all_words, all_moves = get_all_words_and_moves(word_dictionary, player2_tiles, played_moves_to_points.keys())

    move_for_most_points, most_points = get_most_points_in_dictionary(all_moves)

    print(move_for_most_points, most_points)

    #Need to build a checker system to see if there are word conflicts now

    

    



    
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