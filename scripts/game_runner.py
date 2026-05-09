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

def minimax_search(game: Game, state, depth):
    player = game.to_move(state)
    value, move = max_value(game, state, depth, player)
    return move

def max_value(game: Game, state, depth, player):
    if game.is_cutoff(depth):
        return game.eval(state, player), None
    
    v = float('-inf')
    move = None

    for action in game.actions(state):
        v2, a2 = min_value(game, game.result(state, action), depth + 1, player)
        if v2 > v:
            v, move = v2, action

    return v, move

def min_value(game: Game, state, depth, player):
    if game.is_cutoff(depth):
        return game.eval(state, player), None
    
    v = float('inf')
    move = None

    for action in game.actions(state):
        v2, a2 = max_value(game, game.result(state, action), depth + 1, player)
        if v2 < v:
            v, move = v2, action

    return v, move



def testing():
    game = Game()
    initial_state = game.snapshot()
    game.show_state(initial_state)
    best_move = minimax_search(game, initial_state, 0)
    print(best_move)
    


def main():
    testing()

if __name__ == "__main__":
    cProfile.run('main()')