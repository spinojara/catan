#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "util.c"
#include "position.c"
#include "move.c"
#include "search.c"

int main(void) {
	uint64_t seed = 127387412 + time(NULL);

	struct position pos;

	position_init(&pos);
	randomize_position(&pos, &seed);

	struct node *root = calloc(1, sizeof(*root));

	int bot = 1;

	position_print(&pos);

	while (1) {
		struct move move;

		if (pos.dice) {
			int m = uniformint(&seed, 1, 7) + uniformint(&seed, 1, 7);
			printf("\n\n\n\nchance move: %d\n", m);

			do_random(&pos, m);

			root = free_node(root, m - 2);
		}
		/* This is the steal phase which is random. */
		else if (pos.seven == 1) {
			struct move *moves = NULL;
			size_t n = 0;
			generate(&pos, &moves, &n);

			struct player *them = &pos.player[1 - pos.turn];
			int sum = them->resources[BRICK] + them->resources[LUMBER] + them->resources[GRAIN] + them->resources[ORE] + them->resources[WOOL];
			size_t j;
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

			printf("move: ");
			print_move(&pos, &moves[j]);
			printf("\n");

			do_move(&pos, &moves[j]);

			root = free_node(root, j);
		}
		else {
			if (pos.turn == bot) {
				mcts(&pos, 7 * 1000, &root, &seed, &move);

				printf("choosing: ");
				print_move(&pos, &move);
				printf("\n");
			}
			else {
				struct move *moves = NULL;
				size_t n = 0;
				generate(&pos, &moves, &n);

				size_t i = n;
				while (i == n) {
					printf("make a move\n");

					char buf[4096];
					if (!fgets(buf, sizeof(buf), stdin))
						exit(54);
					buf[strcspn(buf, "\n")] = 0;

					for (i = 0; i < n; i++) {
						char movestr[256];
						move_to_str(&moves[i], movestr);
						if (!strcmp(movestr, buf)) {
							printf("ok\n");
							memcpy(&move, &moves[i], sizeof(move));
							break;
						}
					}

					if (i == n) {
						printf("available moves: \n");
						for (i = 0; i < n; i++) {
							printf("'");
							print_move(&pos, &moves[i]);
							printf("'\n");
						}
					}
				}

				/* Order is deterministic so we can just go to this node. */
				root = free_node(root, i);
			}

			do_move(&pos, &move);
		}

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
}
