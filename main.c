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

	position_print(&pos);

	while (1) {
		int setup = pos.setup;

		struct move move;
		if (pos.turn == 1) {
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
				fgets(buf, sizeof(buf), stdin);
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

		int seven = pos.seven;

		do_move(&pos, &move);

		if ((move.flip && !setup) && !seven) {
			int m = uniformint(&seed, 1, 7) + uniformint(&seed, 1, 7);
			printf("chance move: %d\n", m);

			do_random(&pos, m);

			root = free_node(root, m - 2);
		}
	}
}
