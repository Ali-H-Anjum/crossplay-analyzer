from game import Game
import random
import math

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

        print(f"Alpha-Beta Nodes explored: {self.nodes_explored} \n {move}")
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
    




class MCTSNode:
    """A node in the MCTS tree."""

    def __init__(self, state, parent=None, action=None):
        self.state = state
        self.parent = parent
        self.action = action        # action that led to this node
        self.children = []
        self.visits = 0
        self.wins = 0
        self.untried_actions = None  # filled on first expansion

    def ucb1(self, C=1.41):
        """Upper Confidence Bound for Trees."""
        if self.visits == 0:
            return float('inf')
        return (self.wins / self.visits) + C * math.sqrt(
            math.log(self.parent.visits) / self.visits
        )


class MCTSAgent:
    """Monte Carlo Tree Search agent."""

    def __init__(self, game, n_simulations=1000, balance_constant = 1.41):
        self.game = game
        self.n_simulations = n_simulations
        self.balance_constant = balance_constant

    def search(self, state):
        """Return the best action after running MCTS."""
        root = MCTSNode(state)
        root.untried_actions = list(self.game.actions(state))

        for i in range(self.n_simulations):
            node = self._select(root)
            node = self._expand(node)
            result = self._simulate(node.state)
            print(i)
            self._backpropagate(node, result)
            

        # pick child with most visits (most robust choice)
        best_child = max(root.children, key=lambda c: c.visits)
        return best_child.action

    def _select(self, node):
        """Walk down the tree using UCB1 until we find an expandable node."""
        while node.untried_actions is not None and len(node.untried_actions) == 0:
            if not node.children:
                return node
            node = max(node.children, key=lambda c: c.ucb1(self.balance_constant))
        return node

    def _expand(self, node):
        """Add one new child to the node."""
        if node.untried_actions is None:
            node.untried_actions = list(self.game.actions(node.state))
        if not node.untried_actions or self.game.is_terminal(node.state):
            return node
        action = node.untried_actions.pop()
        new_state = self.game.result(node.state, action)
        child = MCTSNode(new_state, parent=node, action=action)
        child.untried_actions = list(self.game.actions(new_state))
        node.children.append(child)
        return child

    def _simulate(self, state):
        """Play randomly from state to a terminal state. Return utility for player 1."""
        while not self.game.is_terminal(state):
            if self.game.actions(state) == set():
                break
            action = random.choice(list(self.game.actions(state)))
            state = self.game.result(state, action)
        return self.game.utility(state, 1)

    def _backpropagate(self, node, result):
        """Update visits and wins up the tree."""
        while node is not None:
            node.visits += 1
            # count as a win if the result favors the player who just moved
            player_at_node = self.game.to_move(node.state)
            if result == -player_at_node:  # the player who moved TO this node won
                node.wins += 1
            elif result == 0:
                node.wins += 0.5  # draws count as half
            node = node.parent

    