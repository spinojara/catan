#!/usr/bin/env python3

# Plays one game of catan against itself for CLOP, and prints W, L or D for
# the engine with the parameters. Called like clop-reverseplay.py:
#
#   clop-catan.py ms setup_ms processor seed name value name value ...
#
# where ms is the time per move, setup_ms the time for each of the setup
# moves, and the rest is added by CLOP. The board is made from seed / 2, and
# seed % 2 decides who starts. See gamemaster.py.

import sys

from gamemaster import make_board, play

argv = sys.argv[1:]

if len(argv) < 4 or len(argv) % 2 != 0:
    sys.stderr.write('Too few arguments\n')
    sys.exit(2)

ms = argv[0]
setup_ms = argv[1]
clop_seed = int(argv[3])

engine = ['./catanclop']
tuned = ['./catanclop'] + ['%s=%s' % (argv[i], argv[i + 1]) for i in range(4, len(argv) - 1, 2)]

# The engine with the parameters starts when the seed is odd.
tuned_is_first = clop_seed % 2 != 0
first, second = (tuned, engine) if tuned_is_first else (engine, tuned)

try:
    result = play(first, second, make_board(clop_seed // 2), ms, setup_ms)
except RuntimeError as error:
    sys.stderr.write('error: %s\n' % error)
    sys.exit(1)

if result == 0:
    sys.stdout.write('D\n')
else:
    sys.stdout.write('W\n' if (result > 0) == tuned_is_first else 'L\n')
