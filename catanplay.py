#!/usr/bin/env python3

# Plays ./catan1 against ./catan2 many times, like reverseplay.py, and prints
# wins - draws - losses for catan1 after each game. Game i is played on board
# i / 2, and catan1 starts when i is even, so every board is played twice
# with the other engine starting. See gamemaster.py.

from concurrent.futures import ThreadPoolExecutor, as_completed
import sys

from gamemaster import make_board, play

GAMES = 1000
GAMES_AT_ONCE = 5
MS = 100
# Time for each of the setup moves.
SETUP_MS = 4000

# The result of game i for catan1, or None if it went wrong.
def game(i):
    try:
        if i % 2 == 0:
            return play(['./catan1'], ['./catan2'], make_board(i // 2), MS, SETUP_MS)
        return -play(['./catan2'], ['./catan1'], make_board(i // 2), MS, SETUP_MS)
    except RuntimeError as error:
        sys.stderr.write('error: game %d: %s\n' % (i, error))
        return None

stats = [0, 0, 0]

executor = ThreadPoolExecutor(GAMES_AT_ONCE)
try:
    for future in as_completed([executor.submit(game, i) for i in range(GAMES)]):
        result = future.result()
        if result is None:
            continue
        # A win, draw or loss for catan1 is 1, 0 or -1.
        stats[1 - result] += 1
        print(f'{stats[0]} - {stats[1]} - {stats[2]}', flush=True)
finally:
    # Do not start the remaining games if we were stopped.
    executor.shutdown(cancel_futures=True)
