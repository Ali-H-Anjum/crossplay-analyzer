class Move:
    def __init__(self, word, x, y, is_descending):
        if not self.in_bounds(x, y):
            raise ValueError(f"Position ({x}, {y}) is out of bounds. Must be between 0 and 14.")
        
        x_end, y_end = self.get_endpoints(word, x, y, is_descending)

        if not self.in_bounds(x_end, y_end):
            raise ValueError(f"End position ({x_end}, {y_end}) is out of bounds. Word '{word}' doesn't fit on board.")

        self.__word = word
        self.__x = x
        self.__y = y
        self.__is_descending = is_descending
        self.__x_end = x_end
        self.__y_end = y_end

    def get_word(self):
        return self.__word
    
    def get_x(self):
        return self.__x
    
    def get_y(self):
        return self.__y
    
    def get_is_descending(self):
        return self.__is_descending
    
    def get_x_end(self):
        return self.__x_end
    
    def get_y_end(self):
        return self.__y_end
    
    def get_length(self):
        return len(self.__word)
    
    def get_letter_at_displacement(self, n):
        if(n < 0 or n > self.get_length() - 1):
            raise ValueError(f"Displacement ({n}) is out of bounds. Must be positive and shorter than the word length")
        return self.__word[n]
    
    def get_endpoints(self, word, x, y, is_descending):
        length = len(word)
        if(is_descending): return x, y - length + 1
        else: return x + length - 1, y
    
    def in_bounds(self, x, y):
        return 0 <= x <= 14 and 0 <= y <= 14

##################################################
#m = Move("hello", 5, 5, False)

#print(m.get_x_end(), m.get_y_end(), m.get_length(), m.get_letter_at_displacement(0))