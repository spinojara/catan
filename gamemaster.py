# The game master for two catan engines in self play (see main.c). It sends
# the board to both engines and passes their moves to each other. Used by
# clop-catan.py and catanplay.py.

from subprocess import Popen, PIPE
import random
import threading

# A game that goes on longer than this many lines is cut off.
MAX_LINES = 20000

# The same kind of board as randomize_position in position.c, as the lines
# that catan -b reads.
def make_board(seed):
    rng = random.Random(seed)
    numbers = [2, 3, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 10, 11, 11, 12]
    resources = ['brick'] * 3 + ['lumber'] * 4 + ['ore'] * 3 + ['grain'] * 4 + ['wool'] * 4
    rng.shuffle(numbers)
    rng.shuffle(resources)
    tiles = ['%d %s' % tile for tile in zip(numbers, resources)]
    tiles.insert(rng.randrange(19), '0 desert')
    return tiles

# Plays one game on the board, which first starts. first and second are an
# engine followed by its parameters, like ['./catan', 'c=1.2']. The first
# player makes up the dice rolls, steals and drawn cards, and the second
# player reads them. ms is the time per move, and setup_ms the time for each
# of the setup moves. Returns 1 if first won, -1 if second won and 0 if the
# game was cut off.
def play(first, second, board, ms, setup_ms):
    times = ['-t', str(ms), '-T', str(setup_ms)]
    commands = [first[:1] + ['-s', '-b'] + times + first[1:],
                second[:1] + ['-s', '-b', '-m', '-2'] + times + second[1:]]
    players = [Popen(command, stdin=PIPE, stdout=PIPE, stderr=PIPE, text=True, bufsize=1)
               for command in commands]

    for player in players:
        for line in board:
            player.stdin.write(line + '\n')
        player.stdin.flush()

    lock = threading.Lock()
    lines = 0
    capped = False
    results = [None, None]
    errors = []

    # Pass everything one engine prints on to the other one.
    def relay(source, destination):
        nonlocal lines, capped
        for line in source.stdout:
            with lock:
                lines += 1
                if lines > MAX_LINES:
                    capped = True
                    for player in players:
                        player.kill()
                    return
            try:
                destination.stdin.write(line)
                destination.stdin.flush()
            except (BrokenPipeError, ValueError):
                return
        # The source is gone, so the destination must not wait for it.
        try:
            destination.stdin.close()
        except (BrokenPipeError, ValueError):
            pass

    # stderr has how each engine rates its moves, who won at the end, and
    # why it stopped if something went wrong.
    def read_result(i):
        for line in players[i].stderr:
            line = line.strip()
            if line in ('robot wins', 'human wins'):
                results[i] = line
            elif line.startswith('error'):
                errors.append('%s: %s' % (commands[i][0], line))

    threads = [threading.Thread(target=relay, args=(players[0], players[1])),
               threading.Thread(target=relay, args=(players[1], players[0])),
               threading.Thread(target=read_result, args=(0,)),
               threading.Thread(target=read_result, args=(1,))]
    for thread in threads:
        thread.start()
    for player in players:
        player.wait()
    for thread in threads:
        thread.join()

    # A pipe to an engine that died can still hold a line, which would
    # otherwise give a warning when Python cleans it up.
    for player in players:
        for pipe in (player.stdin, player.stdout, player.stderr):
            try:
                pipe.close()
            except OSError:
                pass

    if capped:
        return 0

    # Both engines must agree on who won.
    if results[0] is None or results[1] is None or results[0] == results[1]:
        raise RuntimeError('no result (%s, %s)%s' % (results[0], results[1],
                                                     ''.join('\n  ' + error for error in errors)))

    return 1 if results[0] == 'robot wins' else -1
