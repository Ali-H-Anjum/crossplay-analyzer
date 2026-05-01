from Game import Game
import cProfile

def simulate_one_game():
    game = Game()
    while len(game.get_tile_bag().get_tilebag()) > 0:
        game.play_turn()
    game.play_turn()
    game.play_turn()
    print("Game Over")

def main():
    simulate_one_game()

if __name__ == "__main__":
    cProfile.run('main()')