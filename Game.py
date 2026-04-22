from Move import Move
from Board import Board
from Tiles import Tiles
from MoveGenerator import MoveGenerator
from TileBag import Tilebag
from Player import Player
from MoveEvaluator import MoveEvaluator
from gaddagTesting import GADDAG, GADDAGNode
from Validator import Validator

import pickle

import cProfile

class Game:
    def __init__(self):
        
        self._gaddag = GADDAG()

        self._moveGenerator = MoveGenerator(self._gaddag)

        self._tileBag = Tilebag()
        self._players = [Player(Tiles(self._tileBag.draw_tiles(7))), Player(Tiles(self._tileBag.draw_tiles(7)))]
        self._board = Board()

        self._moveEvaluator = MoveEvaluator(self._board)

        self._current_player_index = 0

        self._validator = Validator()

        print("Game Begin")

    def play_turn(self):
        current_player = self.get_current_player()
        tiles = current_player.get_player_tiles()

        print("Player " + str(self._current_player_index) + " has " + str(tiles))

        moves = self._moveGenerator.get_all_moves(self._board, tiles)

        if not moves:
            print("No moves found")
            self.swap_players()
            return

        points_per_move = self._moveEvaluator.sort_by_points(moves)
        highest_moves = self._moveEvaluator.get_top_number(points_per_move, 15)

        # print("Their higher moves are:")
        # for move in highest_moves:
        #     print(move)

        most_points, move_with_most_points = highest_moves[0]

        tiles_used = current_player.use_tiles_for_move(self._board, move_with_most_points)

        self._board.add_move(move_with_most_points)

        current_player.add_score(most_points)

        print("They play (" + str(move_with_most_points) + ") for " + str(most_points) + " points")
        self._board.show_board()

        tiles_needed = 7 - len(current_player.get_player_tiles())
        current_player.add_tiles(self._tileBag.draw_tiles(tiles_needed))

        #print("Their new tiles are " + str(current_player.get_player_tiles()))

        self.swap_players()

        #self._validator.check_board(self._board.get_board())

    def get_current_player(self):
        return self._players[self._current_player_index]
    
    def swap_players(self):
        self._current_player_index = 1 - self._current_player_index



#b.add_word('TOWER', 5, 7, False)
#b.add_word('HOW', 7, 9, True)

#m0 = Move('HELLO', 7, 0, False)
#m1 = Move('BRAILE', 8, 5, True)
#m2 = Move('JELLO', 11, 4, True)
#b.add_word(m0.get_word(), m0.get_x(), m0.get_y(), m0.get_is_descending())
#b.add_word(m1.get_word(), m1.get_x(), m1.get_y(), m1.get_is_descending())
#b.add_word(m2.get_word(), m2.get_x(), m2.get_y(), m2.get_is_descending())

#We have to expand horizontally and then vertically from each edge
#When doing vertical expansion we use horizontal constraints
#When doing horizontal expansion we use vertical constraints

##########################################################################
def main():
    game = Game()
    game.play_turn()
    game.play_turn()
    game.play_turn()
    game.play_turn()
    game.play_turn()




    


if __name__ == "__main__":
    cProfile.run('main()')
    





