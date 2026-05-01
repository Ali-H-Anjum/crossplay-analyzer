from Move import Move
from Board import Board
from Tiles import Tiles
from MoveGenerator import MoveGenerator
from TileBag import Tilebag
from Player import Player
from MoveEvaluator import MoveEvaluator
from gaddagTesting import GADDAG, GADDAGNode

class Game:
    def __init__(self):
        self._gaddag = GADDAG()

        self._moveGenerator = MoveGenerator(self._gaddag)

        self._tileBag = Tilebag()
        self._players = [Player(Tiles(self._tileBag.draw_tiles(7))), Player(Tiles(self._tileBag.draw_tiles(7)))]
        self._board = Board()

        self._moveEvaluator = MoveEvaluator(self._board)

        self._current_player_index = 0

        print("Game Begin")
        print()

    def get_tile_bag(self):
        return self._tileBag

    def play_turn(self):
        current_player = self.get_current_player()
        tiles = current_player.get_player_tiles()

        print("Player " + str(self._current_player_index) + " has " + str(tiles))
        print()

        moves = self._moveGenerator.get_all_moves(self._board, tiles)

        if not moves:
            print("No moves found")
            self.swap_players()
            return

        points_per_move = self._moveEvaluator.sort_by_points(moves)

        word_finder_moves = self._moveEvaluator.sort_by_word_finder(points_per_move)

        # print("Their top 40 moves are:")
        # for move in word_finder_moves:
        #     print(move)
        # print()

        most_points, move_with_most_points = word_finder_moves[0]

        tiles_used = current_player.use_tiles_for_move(self._board, move_with_most_points)

        self._board.add_move(move_with_most_points)

        current_player.add_score(most_points)

        print("They play (" + str(move_with_most_points) + ") for " + str(most_points) + " points")
        self._board.show_board()
        print()

        print("They now have " + str(current_player.get_score()) + " points")
        print()

        tiles_needed = 7 - len(current_player.get_player_tiles())
        current_player.add_tiles(self._tileBag.draw_tiles(tiles_needed))

        #print("Their new tiles are " + str(current_player.get_player_tiles()))

        self.swap_players()

    def get_current_player(self):
        return self._players[self._current_player_index]
    
    def swap_players(self):
        self._current_player_index = 1 - self._current_player_index

    





