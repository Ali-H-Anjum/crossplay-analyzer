class Move:
    def __init__(self, word, x, y, is_descending):
        self._word = word
        self._x = x
        self._y = y
        self._is_descending = is_descending

        self._letter_positions = self.calculate_letter_positions()

    def get_word(self):
        return self._word
    
    def get_x(self):
        return self._x
    
    def get_y(self):
        return self._y
    
    def get_is_descending(self):
        return self._is_descending
    
    def get_letter_positions(self):
        return self._letter_positions
    
    def get_length(self):
        return len(self._word)    

    def calculate_letter_positions(self): 
        letter_positions = []
        x, y = self._x, self._y

        for letter in self._word:
            letter_positions.append((letter, x, y))
            if self._is_descending:
                y -= 1
            else:
                x += 1

        return tuple(letter_positions)

##################################################
    def __eq__(self, other):
        return (self.get_word() == other.get_word() and self.get_x() == other.get_x() and self.get_y() == other.get_y() and self.get_is_descending() == other.get_is_descending())

    def __hash__(self):
        return hash((self.get_word(), self.get_x(), self.get_y(), self.get_is_descending()))
    
    def __str__(self):
        return "Word: " + self.get_word() + ", Starting Position: (" + str(self.get_x()) + ", " + str(self.get_y()) + "), Descending: " + str(self.get_is_descending())
    
    def __lt__(self, other): #Treats moves with the same points the same
        return False
    
    def __repr__(self): #Lets me print the whole set/list
        return self.__str__()