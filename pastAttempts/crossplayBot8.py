from collections import Counter

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

def in_bounds(x, y):
    if((x < 0) or (x > 14) or (y < 0) or (y > 14)):
        return False
    else:
        return True
    
def word_in_bounds(word, x, y, is_descending):
    x_end, y_end = get_word_endpoint(word, x, y, is_descending)

    return(in_bounds(x, y) and in_bounds(x_end, y_end))

def get_word_endpoint(word, x, y, is_descending):
    length = len(word)
    if(is_descending):
        return x, y - length + 1
    else:
        return x + length - 1, y

def has_parrallel_intersection(word1, x1, y1, is_descending1, word2, x2, y2, is_descending2):
    x1_end, y1_end = get_word_endpoint(word1, x1, y1, is_descending1)
    x2_end, y2_end = get_word_endpoint(word2, x2, y2, is_descending2)
    
    if (is_descending1 and is_descending2): 
        return (x1 == x2) and ((y1_end <= y2) and (y2_end <= y1))
    if ((not is_descending1) and (not is_descending2)):
        return (y1 == y2) and ((x1 <= x2_end) and (x2 <= x1_end))
    
def has_perpendicular_intersection(word1, x1, y1, is_descending1, word2, x2, y2, is_descending2):
    x1_end, y1_end = get_word_endpoint(word1, x1, y1, is_descending1)
    x2_end, y2_end = get_word_endpoint(word2, x2, y2, is_descending2)

    if (is_descending1 and (not is_descending2)):
        if ((x2 <= x1) and (x1 <= x2_end) and (y1_end <= y2) and (y2 <= y1)):
            return x1, y2
        
    elif ((not is_descending1) and is_descending2):
        if ((x1 <= x2) and (x2 <= x1_end) and (y2_end <= y1) and (y1 <= y2)):
            return x2, y1
        
def get_letter_at_position(word, x, y, is_descending, check_x, check_y):
    if is_descending:
        if check_x == x and (y - len(word) + 1 <= check_y <= y):
            return word[y - check_y]
    else:
        if check_y == y and (x <= check_x <= x + len(word) - 1):
            return word[check_x - x]
        
def single_letter_expansion(word_dictionary, preprocessed_tiles, played_moves):
    legal_moves = []

    tiles = [t.lower() for t in preprocessed_tiles]
    number_of_tiles = len(tiles)

    tile_counts = Counter(tiles)
    blank_count = tile_counts.get('?', 0)
    blanks_needed = 0

    for signature, potential_expansions in word_dictionary.items(): 
        if(len(signature) > number_of_tiles + 1): #Legal move has to be bigger than owned tiles plus one
            continue

        for played_move in played_moves:
            word, x, y, is_descending = played_move

            for i, letter in enumerate(word):
                if(letter not in signature): #Legal move must have the letter
                    continue

                needed_letter_counts = Counter(signature) - Counter(letter)
                possible = True

                for needed_letter, needed_letter_count in needed_letter_counts.items(): #Legal move must have the needed_letters in the tiles
                    owned_tile_count = tile_counts.get(needed_letter, 0)

                    if needed_letter_count > owned_tile_count:
                        blanks_needed += needed_letter_count - owned_tile_count

                        if(blanks_needed > blank_count):
                            possible = False
                            break

                if not possible:
                    continue

                for potential_expansion in potential_expansions: #Finding the positions of all the possible moves
                    potential_expansion_is_descending = not is_descending
                    letter_position = potential_expansion.find(letter)
                    if(not potential_expansion_is_descending):
                        potential_expansion_x = x - letter_position
                        potential_expansion_y = y - i
                    else:
                        potential_expansion_x = x + i
                        potential_expansion_y = y + letter_position

                    if (not word_in_bounds(potential_expansion, potential_expansion_x, potential_expansion_y, potential_expansion_is_descending)):
                       continue

                    #Check for collisions here (Might become a method)

                    for check_move in played_moves:
                        if check_move == played_move:
                            continue

                        move_word, move_x, move_y, move_is_descending = check_move

                        if has_perpendicular_intersection(potential_expansion, potential_expansion_x, potential_expansion_y, potential_expansion_is_descending, move_word, move_x, move_y, move_is_descending): #Legal move must be acceptable with played moves
                            x_of_intersection, y_of_intersection = has_perpendicular_intersection(potential_expansion, potential_expansion_x, potential_expansion_y, potential_expansion_is_descending, move_word, move_x, move_y, move_is_descending)

                            if get_letter_at_position(potential_expansion, potential_expansion_x, potential_expansion_y, potential_expansion_is_descending, x_of_intersection, y_of_intersection) != get_letter_at_position(move_word, move_x, move_y, move_is_descending, x_of_intersection, y_of_intersection):
                                continue

                        #Following require complex word checks so skip for now since its more likely they will fail the word checks anyway
                        if has_parrallel_intersection(potential_expansion, potential_expansion_x, potential_expansion_y, potential_expansion_is_descending, move_word, move_x, move_y, move_is_descending):
                            continue

                    legal_moves.append((potential_expansion, potential_expansion_x, potential_expansion_y, not is_descending))
                    print((potential_expansion, potential_expansion_x, potential_expansion_y, not is_descending))


    print(legal_moves)                        






                

        
                







######################################################
wd = create_word_dictionary()
t = ['A', 'L', 'I', 'G', 'D', 'H', 'I']
pm = [('haj', 7, 7, True)]


single_letter_expansion(wd, t, pm)

#print(has_perpendicular_intersection('gourmet', 7, 7, False, 'brother', 10, 7, False))