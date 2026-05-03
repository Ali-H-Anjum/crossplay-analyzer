from board import Board
from tiles import Tiles
from move_generator import MoveGenerator
from tile_bag import Tilebag
from player import Player
from move_evaluator import MoveEvaluator
from gaddag import GADDAG, GADDAGNode

class Game:
    def __init__(self):
        self._gaddag = GADDAG()

        self._moveGenerator = MoveGenerator(self._gaddag)

        self._tileBag = Tilebag()
        self._players = [Player(self._tileBag.draw_tiles(7)), Player(self._tileBag.draw_tiles(7))]
        self._board = Board()

        self._moveEvaluator = MoveEvaluator(self._board)

        self._current_player_index = 0

        self._initial_state = self.get_state()

        print("Game Begin")
        print()

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

        self.use_tiles(current_player, move_with_most_points)

        self._board.add_move(move_with_most_points)

        current_player.add_score(most_points)

        print("They play (" + str(move_with_most_points) + ") for " + str(most_points) + " points")
        self._board.show_board()
        print()

        print("They now have " + str(current_player.get_score()) + " points")
        print()

        tiles_needed = current_player.tiles_needed()
        current_player.add_tiles(self._tileBag.draw_tiles(tiles_needed))

        #print("Their new tiles are " + str(current_player.get_player_tiles()))

        self.swap_players()

    def get_current_player(self):
        return self._players[self._current_player_index]
    
    def swap_players(self):
        self._current_player_index = 1 - self._current_player_index

    def has_tiles_remaining(self):
        return len(self._tileBag) > 0
    
    def use_tiles(self, player, move):
        for letter, x, y in move.get_letter_positions():
            if self._board.get_letter_at_point(x, y):
                continue
            if letter in player.get_player_tiles().get_tiles():
                player.remove_tile(letter)
            else:
                player.remove_tile('?')

    ####### AI METHODS #######

    def _get_board_state(self):
        return self._board.get_board()
    
    def _get_tile_trays_state(self):
        return tuple(player.get_player_tiles().get_tiles() for player in self._players)
    
    def _get_tile_bag_state(self):
        return self._tileBag.get_tilebag()
    
    def _get_scores_state(self):
        return tuple(player.get_score() for player in self._players)
    
    def _get_current_player_state(self):
        return self._current_player_index
    
    def get_state(self):
        return (self._get_board_state(), self._get_tile_trays_state(), self._get_tile_bag_state(), self._get_scores_state(), self._get_current_player_state())
    

    def get_initial_state(self):
        return self._initial_state
    
    def to_move(self, state):
        return state[4]
    
    def actions(self, state):
        board, tile_trays, tile_bag, scores, current_player = state

        temp_board = Board()
        temp_board.set_board(board)

        temp_tray = Tiles(tile_trays[current_player])

        return self._moveGenerator.get_all_moves(temp_board, temp_tray)
    
    def result(self, state, action):
        board, tile_trays, tile_bag, scores, current_player = state

        temp_board = Board()
        temp_board.set_board(board)

        temp_board.add_move(action)







        



    





