from game import Game
from minimax import Depth_Limited_Minimax_Agent, OrderedAlphaBetaAgent, MCTSAgent
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

def testing(): #TODO build complete testing code like from lecture04 HW
    game = Game()
    initial_state = game.snapshot()
    game.show_state(initial_state)

    agent1 = OrderedAlphaBetaAgent(game, 2)
    move1, value1 = agent1.search(initial_state)

    # agent2 = Depth_Limited_Minimax_Agent(game, 2)
    # move2, value2 = agent2.search(initial_state)

    # agent3 = MCTSAgent(game)
    # move3 = agent3.search(initial_state)

    




def main():
    testing()

if __name__ == "__main__":
    cProfile.run('main()')