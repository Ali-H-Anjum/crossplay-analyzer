all_words = list()
with open('wordList.txt', 'r') as file:
    for line in file:
        for word in line.strip().split():
            all_words.append(word)

all_words.sort()
    
with open('alphabetical_word_list.txt', 'w') as file:
    for word in all_words:
        file.write(word + "\n")
        

