from game import Game

class Depth_Limited_Minimax_Agent: 
    def __init__(self, game: Game, max_depth):
        self.game = game
        self.max_depth = max_depth
        self.nodes_explored = 0

    def evaluate(self, state):
        board_snapshot, tile_bag_snapshot, player0_snapshot, player1_snapshot, self._current_player_index, self._turns_since_tilebag_empty = state
        score_difference = player0_snapshot[1] - player1_snapshot[1]

        if self._current_player_index == 0:
            return score_difference
        else:
            return -score_difference

    def search(self, state):
        self.nodes_explored = 0
        player = self.game.to_move(state)
        if player == 0:
            value, move = self.max_value(state, 0)
        else:
            value, move = self.min_value(state, 0)

        print(f"Minimax Nodes explored: {self.nodes_explored} {move}")
        return move, value
    
    def max_value(self, state, depth):
        self.nodes_explored += 1

        if depth == self.max_depth:
            return self.evaluate(state), None
        
        maximum_value = -float('inf')
        best_action = None

        for action in self.game.actions(state):
            child_value, child_action = self.min_value(self.game.result(state, action), depth + 1)
            if child_value > maximum_value:
                maximum_value = child_value
                best_action = action

        return maximum_value, best_action
    
    def min_value(self, state, depth):
        self.nodes_explored += 1
        
        if depth == self.max_depth:
            return self.evaluate(state), None
        
        minimum_value = float('inf')
        best_action = None 

        for action in self.game.actions(state):
            child_value, child_action = self.max_value(self.game.result(state, action), depth + 1)
            if child_value < minimum_value:
                minimum_value = child_value
                best_action = action

        return minimum_value, best_action


class OrderedAlphaBetaAgent:
    """Alpha-Beta with move ordering for better pruning."""

    def __init__(self, game: Game, max_depth, reverse = False):
        self.game = game
        self.max_depth = max_depth
        self.reverse = reverse  # if True, use worst-first ordering
        self.nodes_explored = 0

    def evaluate(self, state):
        board_snapshot, tile_bag_snapshot, player0_snapshot, player1_snapshot, self._current_player_index, self._turns_since_tilebag_empty = state
        score_difference = player0_snapshot[1] - player1_snapshot[1]

        if self._current_player_index == 0:
            return score_difference
        else:
            return -score_difference

    def order_actions(self, state, actions, maximizing): #Ranks moves by searching 1 ply forward
        """Sort actions by heuristic value of the resulting state."""

        #lambda is a way to write a function without a name, great for sorting
        return sorted(actions, key=lambda action: self.evaluate(self.game.result(state, action)), reverse = maximizing != self.reverse)

    def search(self, state):
        self.nodes_explored = 0
        player = self.game.to_move(state)
        if player == 0:
            value, move = self.max_value(state, 0, -float('inf'), float('inf'))
        else:
            value, move = self.min_value(state, 0, -float('inf'), float('inf'))

        print(f"Alpha-Beta Nodes explored: {self.nodes_explored} {move}")
        return move, value

    def max_value(self, state, depth, alpha, beta):
        self.nodes_explored += 1

        if depth == self.max_depth:
            return self.evaluate(state), None
        
        maximum_value = -float('inf')
        best_action = None

        actions = self.order_actions(state, self.game.actions(state), maximizing = True)

        for action in actions:
            child_value, child_action = self.min_value(self.game.result(state, action), depth + 1, alpha, beta)
            if child_value > maximum_value:
                maximum_value = child_value
                best_action = action

            alpha = max(alpha, maximum_value)
            if maximum_value >= beta:
                break

        return maximum_value, best_action

    def min_value(self, state, depth, alpha, beta):
        self.nodes_explored += 1

        if depth == self.max_depth:
            return self.evaluate(state), None
        
        minimum_value = float('inf')
        best_action = None

        actions = self.order_actions(state, self.game.actions(state), maximizing = False)

        for action in actions:
            child_value, child_action = self.max_value(self.game.result(state, action), depth + 1, alpha, beta)
            if child_value < minimum_value:
                minimum_value = child_value
                best_action = action

            beta = min(beta, minimum_value)
            if minimum_value <= alpha:
                break

        return minimum_value, best_action
    