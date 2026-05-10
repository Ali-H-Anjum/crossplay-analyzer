from board import Board
from tray import Tray
from move_generator import MoveGenerator
from tile_bag import Tilebag
from player import Player
from move_evaluator import MoveEvaluator
from gaddag import GADDAG, GADDAGNode

class Game:
    def __init__(self):
        self._board = Board()
        self._tileBag = Tilebag()
        self._players = [Player(self._tileBag.draw_tiles(7)), Player(self._tileBag.draw_tiles(7))] #MAX, MIN
        self._current_player_index = 0
        self._turns_since_tilebag_empty = 0

        self._gaddag = GADDAG()
        self._moveGenerator = MoveGenerator(self._gaddag)
        self._moveEvaluator = MoveEvaluator(self._board)

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
    
    def use_tiles(self, player, move): #Should be broken up
        for letter, x, y in move.get_letter_positions():
            if self._board.get_letter_at_point(x, y):
                continue
            if letter in player.get_player_tiles().get_tiles():
                player.remove_tile(letter)
            else:
                player.remove_tile('?')

    ##################### AI METHODS #####################
    
    def snapshot(self):
        return (
            self._board.snapshot(), #Tuple grid board, Tuple anchor positions, Tuple blank positions
            self._tileBag.snapshot(), #Tuple tileBag
            self._players[0].snapshot(), #Tuple first player tiles, Int first player score
            self._players[1].snapshot(), #Tuple second player tiles, Int second player score
            self._current_player_index, # Int current player index
            self._turns_since_tilebag_empty
        )
    
    def restore(self, game_snapshot):
        board_snapshot, tile_bag_snapshot, player0_snapshot, player1_snapshot, self._current_player_index, self._turns_since_tilebag_empty = game_snapshot

        self._board.restore(board_snapshot)
        self._tileBag.restore(tile_bag_snapshot)
        self._players[0].restore(player0_snapshot)
        self._players[1].restore(player1_snapshot)
    
    def to_move(self, game_snapshot):
        return game_snapshot[4]
    
    def actions(self, game_snapshot):
        original = self.snapshot() #Save original state

        self.restore(game_snapshot) #Load new state

        current_player = self.get_current_player()
        tiles = current_player.get_player_tiles()
        moves = self._moveGenerator.get_all_moves(self._board, tiles)

        self._moveEvaluator.set_board(self._board)
        points_per_move = self._moveEvaluator.sort_by_points(moves) #Assigns points to moves

        self.restore(original) #Load original state

        return points_per_move
    
    def result(self, game_snapshot, action):
        self.restore(game_snapshot)

        current_player = self.get_current_player()
        self._moveEvaluator.set_board(self._board)
        self._board.add_move(action)

        self.use_tiles(current_player, action)

        points = self._moveEvaluator.calculate_total_points_per_move(action)

        current_player.add_score(points)

        tiles_needed = current_player.tiles_needed()

        current_player.add_tiles(self._tileBag.draw_tiles(tiles_needed)) #Need to hide this information

        if not self.has_tiles_remaining():
            self._turns_since_tilebag_empty += 1

        self.swap_players()

        new_snapshot = self.snapshot()
        self.restore(game_snapshot)

        return new_snapshot
    
    # def is_terminal(self, game_snapshot):
    #     return game_snapshot[5] >= 2

    # def is_cutoff(self, depth):
    #     return depth > 0
    
    def eval(self, game_snapshot, player):
        # self.restore(game_snapshot)

        board_snapshot, tile_bag_snapshot, player0_snapshot, player1_snapshot, self._current_player_index, self._turns_since_tilebag_empty = game_snapshot

        score_difference = player0_snapshot[1] - player1_snapshot[1]

        if player == 0: return score_difference
        else: return -score_difference
        
    
    def show_state(self, game_snapshot):
        original = self.snapshot()
        self.restore(game_snapshot)
        board_snapshot, tile_bag_snapshot, player0_snapshot, player1_snapshot, self._current_player_index, self._turns_since_tilebag_empty = self.snapshot()

        self._board.show_board()

        print("The anchor positions on the board are: " + str(board_snapshot[1]))
        print()

        print("The blank positions on the board are: " + str(board_snapshot[2]))
        print()
        
        print("The tile bag has these tiles: " + str(tile_bag_snapshot))
        print()

        print("Player 0's tray has: " + str(player0_snapshot[0]) + " and they have " + str(player0_snapshot[1]) + " points")
        print()

        print("Player 1's tray has: " + str(player1_snapshot[0]) + " and they have " + str(player1_snapshot[1]) + " points")
        print()

        print("The current player is: " + str(self._current_player_index))
        print()

        print("The number of turns since the tile bag was empty is: " + str(self._turns_since_tilebag_empty))
        print()

        self.restore(original)












        



    





