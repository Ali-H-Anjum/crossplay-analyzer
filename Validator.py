class Validator:
    def __init__(self):
        pass

    def check_board(self, board):
        for row in board:
            for letter in row:
                if letter == ' ':
                    continue

                print(letter)