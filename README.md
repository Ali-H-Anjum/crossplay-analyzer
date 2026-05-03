# Crossplay Analyzer
A Python implementation of the Crossplay word game engine, built as a foundation for exploring AI techniques and behavior.

## Status

The game engine is complete. Currently `game_runner` runs an entire simulated game from start to finish where each player selects the move with the highest score until the game ends.

There are structural and efficiency improvements planned for move generation, but the current focus is designing a minimax-H agent utilizing the engine in its current state.

## Research

The move generation algorithm uses a GADDAG data structure, designed with guidance from academic research papers (see `docs/papers/`).

## Quick Start

```bash
pip install -e .
python scripts/game_runner.py
```

## WordList

This project uses the NASPA Word List (NWL 2023). Due to copyright, the wordlist is not included. Obtain a copy and place it in the project src.

