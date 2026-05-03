from src.game import Game
import cProfile

def simulate_one_game():
    game = Game()
    while game.has_tiles_remaining():
        game.play_turn()
    game.play_turn()
    game.play_turn()
    print("Game Over")

def testing():
    game = Game()
    s0 = game.get_initial_state()
    print(game.actions(s0))


def main():
    simulate_one_game()

if __name__ == "__main__":
    cProfile.run('main()')