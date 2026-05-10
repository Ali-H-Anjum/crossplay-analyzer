from game import Game
import cProfile

def simulate_one_game():
    game = Game()
    print("Game Begin")
    print()
    while game.has_tiles_remaining():
        game.play_turn()
    game.play_turn()
    game.play_turn()
    print("Game Over")

def minimax_search(game: Game, state, max_depth):
    nodes = [0]

    player = game.to_move(state)
    if player == 0: #MAX
        minimax_value, move = max_value(game, state, 0, max_depth, nodes)
    else: #MIN
        minimax_value, move = min_value(game, state, 0, max_depth, nodes)

    print(f"Minimax Nodes explored: {nodes[0]}")
    return minimax_value, move

def max_value(game: Game, state, depth, max_depth, nodes):
    nodes[0] += 1
    if depth == max_depth:
        return game.eval(state, 0), None #Evaluates MAX POV
    
    maximum_value = float('-inf')
    best_action = None

    for points, action in game.actions(state):
        child_value, child_action = min_value(game, game.result(state, action), depth + 1, max_depth, nodes)
        if child_value > maximum_value:
            maximum_value, best_action = child_value, action

    return maximum_value, best_action

def min_value(game: Game, state, depth, max_depth, nodes):
    nodes[0] += 1
    if depth == max_depth:
        return game.eval(state, 0), None #Evaluates MAX POV
    
    minimum_value = float('inf')
    best_action = None

    for points, action in game.actions(state):
        child_value, child_action = max_value(game, game.result(state, action), depth + 1, max_depth, nodes)
        if child_value < minimum_value:
            minimum_value, best_action = child_value, action

    return minimum_value, best_action



def alpha_beta_search(game: Game, state, max_depth):
    nodes = [0]

    player = game.to_move(state)
    if player == 0: #MAX
        minimax_value, move = max_value_ab(game, state, 0, max_depth, float('-inf'), float('inf'), nodes)
    else: #MIN
        minimax_value, move = min_value_ab(game, state, 0, max_depth, float('-inf'), float('inf'), nodes)

    print(f"Alpha-Beta Nodes explored: {nodes[0]}")
    return minimax_value, move

def max_value_ab(game: Game, state, depth, max_depth, alpha, beta, nodes):
    nodes[0] += 1
    if depth == max_depth:
        return game.eval(state, 0), None #Evaluates MAX POV
    
    maximum_value = float('-inf')
    best_action = None

    actions = sorted(game.actions(state), key=lambda a: a[0], reverse=True)

    for points, action in actions: #Top 40 Moves [:40]
        child_value, child_action = min_value_ab(game, game.result(state, action), depth + 1, max_depth, alpha, beta, nodes)
        if child_value > maximum_value:
            maximum_value, best_action = child_value, action

        alpha = max(alpha, maximum_value)
        if maximum_value >= beta:
            return maximum_value, best_action

    return maximum_value, best_action

def min_value_ab(game: Game, state, depth, max_depth, alpha, beta, nodes):
    nodes[0] += 1
    if depth == max_depth:
        return game.eval(state, 0), None #Evaluates MAX POV
    
    minimum_value = float('inf')
    best_action = None

    actions = sorted(game.actions(state), key=lambda a: a[0], reverse=True)

    for points, action in actions: #Bottom 40 Moves [:40]
        child_value, child_action = max_value_ab(game, game.result(state, action), depth + 1, max_depth, alpha, beta, nodes)
        if child_value < minimum_value:
            minimum_value, best_action = child_value, action
        
        beta = min(beta, minimum_value)
        if minimum_value <= alpha:
            return minimum_value, best_action

    return minimum_value, best_action

def testing():
    game = Game()
    initial_state = game.snapshot()
    game.show_state(initial_state)

    # value, best_move = minimax_search(game, initial_state, max_depth = 2)
    # print(best_move)

    value, best_move = alpha_beta_search(game, initial_state, max_depth = 3)
    print(best_move)


def main():
    testing()

if __name__ == "__main__":
    cProfile.run('main()')