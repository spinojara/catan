#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>

#include "util.c"
#include "position.c"
#include "move.c"
#include "search.c"

int selfplay = 0;

void usage(const char *name) {
	fprintf(stderr, "usage: %s [-bms2] [-t ms] [-T ms] [name=value ...]\n"
			"  -b     read the board from stdin: 19 lines like '6 lumber' for\n"
			"         tiles 0 to 18, with '0 desert' for the desert\n"
			"  -m     enter dice rolls, stolen cards and drawn cards instead of\n"
			"         making them up at random\n"
			"  -s     self play: only print what the other copy needs to read\n"
			"  -2     the bot plays second instead of first\n"
			"  -t ms  time to think for each move in milliseconds (default 7000)\n"
			"  -T ms  time to think for each of the setup moves in milliseconds\n"
			"         (default the same as -t)\n"
			"\n"
			"Parameters that can be set, like c=1.2:\n"
			"  c      exploration constant of the search (default 1.41)\n", name);
	exit(1);
}

/* Some moves are generated even though they are not possible. These are things like
 * steals after a 7 and so on. Or card draws.
 */
int possible(struct position *pos, const struct move *move) {
	if (move->type == STEAL) {
		const struct player *them = &pos->player[1 - pos->turn];
		int sum = them->resources[BRICK] + them->resources[LUMBER] + them->resources[GRAIN] + them->resources[ORE] + them->resources[WOOL];

		int r;
		for (r = 0; r < DESERT; r++)
			if (move->in[r])
				break;

		if (!sum || !can_steal(pos))
			return r == DESERT;
		return r != DESERT && them->resources[r] > 0;
	}
	if (move->type == DRAW)
		return pos->development[move->development] > 0;
	return 1;
}

size_t count_possible(struct position *pos, struct move *moves, size_t n) {
	size_t count = 0;
	for (size_t i = 0; i < n; i++)
		count += possible(pos, &moves[i]);
	return count;
}

void send_move(struct move *move) {
	char str[256];
	move_to_str(move, str);
	printf("%s\n", str);
}

size_t read_move(struct position *pos, struct move *moves, size_t n, const char *prompt) {
	while (1) {
		if (!selfplay)
			printf("%s\n", prompt);

		char buf[4096];
		if (!fgets(buf, sizeof(buf), stdin))
			exit(54);
		buf[strcspn(buf, "\n")] = 0;

		for (size_t i = 0; i < n; i++) {
			char movestr[256];
			move_to_str(&moves[i], movestr);
			if (possible(pos, &moves[i]) && !strcmp(movestr, buf)) {
				if (!selfplay)
					printf("ok\n");
				return i;
			}
		}

		if (selfplay) {
			fprintf(stderr, "error: unknown move '%s'\n", buf);
			exit(1);
		}

		printf("available moves: \n");
		for (size_t i = 0; i < n; i++) {
			if (!possible(pos, &moves[i]))
				continue;
			printf("'");
			print_move(pos, &moves[i]);
			printf("'\n");
		}
	}
}

size_t read_event(struct position *pos, struct move *moves, size_t n, const char *prompt) {
	if (count_possible(pos, moves, n) == 1)
		for (size_t i = 0; i < n; i++)
			if (possible(pos, &moves[i]))
				return i;
	return read_move(pos, moves, n, prompt);
}

int main(int argc, char **argv) {
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);

	int manual = 0;
	int ms = 7000;
	/* The same as ms unless -T is given. */
	int setupms = 0;
	int readboard = 0;
	int bot = 1;

	int opt;
	while ((opt = getopt(argc, argv, "bms2t:T:")) != -1) {
		switch (opt) {
		case 'b':
			readboard = 1;
			break;
		case 'm':
			manual = 1;
			break;
		case 's':
			selfplay = 1;
			break;
		case '2':
			bot = 0;
			break;
		case 't': {
			char *end;
			long t = strtol(optarg, &end, 10);
			if (*end || t <= 0 || t > INT_MAX)
				usage(argv[0]);
			ms = t;
			break;
		}
		case 'T': {
			char *end;
			long t = strtol(optarg, &end, 10);
			if (*end || t <= 0 || t > INT_MAX)
				usage(argv[0]);
			setupms = t;
			break;
		}
		default:
			usage(argv[0]);
		}
	}
	if (!setupms)
		setupms = ms;
	/* Parameters as name=value, so that they can be tuned with CLOP. */
	for (int i = optind; i < argc; i++) {
		char *value = strchr(argv[i], '=');
		if (!value)
			usage(argv[0]);
		*value++ = '\0';

		char *end;
		double a = strtod(value, &end);
		if (*end || end == value) {
			fprintf(stderr, "error: bad value '%s' for '%s'\n", value, argv[i]);
			exit(1);
		}

		if (!strcmp(argv[i], "c")) {
			c = a;
		}
		else {
			fprintf(stderr, "error: unknown parameter '%s'\n", argv[i]);
			exit(1);
		}
	}

	uint64_t seed = 127387412 + time(NULL);

	struct position pos;

	position_init(&pos);
	if (readboard) {
		if (!selfplay)
			printf("enter the board, 19 lines like '6 lumber' for tiles 0 to 18\n");
		read_board(&pos);
	}
	else {
		randomize_position(&pos, &seed);
		if (selfplay)
			print_board(&pos);
	}

	struct node *root = calloc(1, sizeof(*root));

	if (!selfplay)
		position_print(&pos);

	while (1) {
		struct move move;

		/* A finished game has no moves, so the search would crash. */
		int result = gameover(&pos);
		if (result) {
			int winner = result > 0 ? pos.turn : 1 - pos.turn;
			fprintf(selfplay ? stderr : stdout, "%s wins\n", winner == bot ? "robot" : "human");
			break;
		}

		if (pos.dice) {
			int m;
			if (manual) {
				struct move *moves = NULL;
				size_t n = 0;
				generate(&pos, &moves, &n);
				m = moves[read_event(&pos, moves, n, "enter the dice roll, like 'roll 8'")].number;
				free(moves);
			}
			else {
				m = uniformint(&seed, 1, 7) + uniformint(&seed, 1, 7);
				if (selfplay)
					printf("roll %d\n", m);
			}
			if (!selfplay)
				printf("\n\n\n\nchance move: %d\n", m);

			do_random(&pos, m);

			struct node *next = free_node(root, m - 2);
			free(root);
			root = next;
		}
		/* Drawing a development card is random too. */
		else if (pos.drawcard) {
			struct move *moves = NULL;
			size_t n = 0;
			generate(&pos, &moves, &n);

			size_t j;
			if (manual) {
				j = read_event(&pos, moves, n, "enter the drawn card, like 'draw knight'");
			}
			else {
				int sum = pos.development[KNIGHT] + pos.development[ROADBUILDING] + pos.development[YEAROFPLENTY] + pos.development[MONOPOLY] + pos.development[VICTORYPOINT];
				int i = uniformint(&seed, 0, sum);
				if (i < pos.development[KNIGHT])
					j = KNIGHT;
				else if (i < pos.development[KNIGHT] + pos.development[ROADBUILDING])
					j = ROADBUILDING;
				else if (i < pos.development[KNIGHT] + pos.development[ROADBUILDING] + pos.development[YEAROFPLENTY])
					j = YEAROFPLENTY;
				else if (i < pos.development[KNIGHT] + pos.development[ROADBUILDING] + pos.development[YEAROFPLENTY] + pos.development[MONOPOLY])
					j = MONOPOLY;
				else
					j = VICTORYPOINT;
			}

			if (!selfplay) {
				printf("move: ");
				print_move(&pos, &moves[j]);
				printf("\n");
			}
			else if (!manual && count_possible(&pos, moves, n) > 1) {
				send_move(&moves[j]);
			}

			do_move(&pos, &moves[j]);

			struct node *next = free_node(root, j);
			free(root);
			root = next;

			free(moves);
		}
		/* This is the steal phase which is random. */
		else if (pos.seven == 1) {
			struct move *moves = NULL;
			size_t n = 0;
			generate(&pos, &moves, &n);

			size_t j;
			if (manual) {
				j = read_event(&pos, moves, n, "enter the stolen card, like 'steal brick'");
			}
			else {
				struct player *them = &pos.player[1 - pos.turn];
				int sum = them->resources[BRICK] + them->resources[LUMBER] + them->resources[GRAIN] + them->resources[ORE] + them->resources[WOOL];
				if (!sum || !can_steal(&pos)) {
					j = DESERT;
				}
				else {
					int i = uniformint(&seed, 0, sum);
					if (i < them->resources[BRICK])
						j = BRICK;
					else if (i < them->resources[BRICK] + them->resources[LUMBER])
						j = LUMBER;
					else if (i < them->resources[BRICK] + them->resources[LUMBER] + them->resources[ORE])
						j = ORE;
					else if (i < them->resources[BRICK] + them->resources[LUMBER] + them->resources[ORE] + them->resources[GRAIN])
						j = GRAIN;
					else
						j = WOOL;
				}
			}

			if (!selfplay) {
				printf("move: ");
				print_move(&pos, &moves[j]);
				printf("\n");
			}
			else if (!manual && count_possible(&pos, moves, n) > 1) {
				send_move(&moves[j]);
			}

			do_move(&pos, &moves[j]);

			struct node *next = free_node(root, j);
			free(root);
			root = next;

			free(moves);
		}
		else {
			if (pos.turn == bot) {
				mcts(&pos, pos.setup ? setupms : ms, &root, &seed, &move, selfplay ? stderr : stdout);

				if (selfplay) {
					send_move(&move);
				}
				else {
					printf("choosing: ");
					print_move(&pos, &move);
					printf("\n");
				}
			}
			else {
				struct move *moves = NULL;
				size_t n = 0;
				generate(&pos, &moves, &n);

				size_t i = read_move(&pos, moves, n, "make a move");
				memcpy(&move, &moves[i], sizeof(move));

				/* Order is deterministic so we can just go to this node. */
				struct node *next = free_node(root, i);
				free(root);
				root = next;

				free(moves);
			}

			do_move(&pos, &move);
		}

		if (selfplay)
			continue;

		printf("human:");
		for (int r = 0; r < DESERT; r++)
			if (pos.player[1 - bot].resources[r])
				printf(" %d %s", pos.player[1 - bot].resources[r], resource_to_string(r));
		printf("\n");

		printf("robot:");
		for (int r = 0; r < DESERT; r++)
			if (pos.player[bot].resources[r])
				printf(" %d %s", pos.player[bot].resources[r], resource_to_string(r));
		printf("\n");
	}

	free_node(root, -1);
	free(root);

	return 0;
}
