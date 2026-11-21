#include <stdint.h>
#include <math.h>

#define TPPERSEC 1000000000l
#define TPPERMS     1000000l

const double c = 1.41;

int64_t time_now(void) {
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
	return (int64_t)tp.tv_sec * TPPERSEC + (int64_t)tp.tv_nsec;
}

struct node {
	int type;

	int w;
	int n;

	char leaf;
	size_t nmoves;
	struct move *moves;
	struct node *nodes;
};

int best_node(const struct node *node) {
	size_t j = 0;
	for (size_t i = 1; i < node->nmoves; i++)
		if (node->nodes[i].n > node->nodes[j].n)
			j = i;

	return j;
}

struct node *free_node(struct node *node, ssize_t except) {
	/* Never expanded, so there is no subtree to keep. Start a new
	 * empty one if a child was asked for.
	 */
	if (!node || !node->n) {
		if (except < 0)
			return NULL;
		struct node *ret = calloc(1, sizeof(*ret));
		if (!ret) {
			fprintf(stderr, "error: calloc\n");
			exit(1);
		}
		return ret;
	}

	if (node->nmoves) {
		for (size_t i = 0; i < node->nmoves; i++) {
			if ((ssize_t)i != except)
				free_node(&node->nodes[i], -1);
		}

		free(node->moves);
	}
	else if (except != 0) {
		free_node(node->nodes, -1);
	}
	struct node *ret = NULL;
	if (except >= 0) {
		ret = malloc(sizeof(*ret));
		if (!ret) {
			fprintf(stderr, "error: malloc\n");
			exit(1);
		}
		memcpy(ret, &node->nodes[except], sizeof(*ret));
	}
	free(node->nodes);

	return ret;
}

int rollout(struct position *pos, uint64_t *seed, int ply) {
	if (ply >= 200)
		return pos->player[pos->turn].score - pos->player[1 - pos->turn].score;
	int result;
	if ((result = gameover(pos)))
		return result;

	struct move *moves = NULL;
	size_t n = 0;

	int type = generate(pos, &moves, &n);

	if (type == CHOICE) {
		/* Filter all roads if we can already reach somewhere.
		 * Maybe allow building longest road, unless we are >= 3 ahead of
		 * our opponent in this category.
		 */
		int has_reachable = 0;
		for (int s = 0; s < MAX_SETTLEMENTS && !has_reachable; s++)
			if (pos->smtiles[s].reachable[pos->turn] && !pos->smtiles[s].blocked && !pos->smtiles[s].city && !pos->smtiles[s].settlement)
				has_reachable = 1;

		if (has_reachable) {
			for (size_t i = 0; i < n; i++) {
				if (moves[i].type == BUILD && moves[i].building == ROAD) {
					memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
					n--;
					i--;
				}
			}
		}

		/* If we can build either a settlement or a city, then do that. */
		int can_build_settlement_or_city = 0;
		for (size_t i = 0; i < n; i++) {
			if (moves[i].type == BUILD && (moves[i].building == CITY || moves[i].building == SETTLEMENT)) {
				can_build_settlement_or_city = 1;
				break;
			}
		}

		if (can_build_settlement_or_city) {
			for (size_t i = 0; i < n; i++) {
				if (moves[i].type != BUILD || !(moves[i].building == CITY || moves[i].building == SETTLEMENT)) {
					memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
					n--;
					i--;
				}
			}
		}

		/* Only allow trades which immediately make us able to build. */
		for (size_t i = 0; i < n; i++) {
			if (moves[i].type != TRADE)
				continue;

			if (!has_reachable && (pos->player[pos->turn].resources[BRICK] + moves[i].in[BRICK] - moves[i].out[BRICK] >= 1) && (pos->player[pos->turn].resources[LUMBER] + moves[i].in[LUMBER] - moves[i].out[LUMBER] >= 1))
				continue;

			if (has_reachable && (pos->player[pos->turn].resources[BRICK] + moves[i].in[BRICK] - moves[i].out[BRICK] >= 1) && (pos->player[pos->turn].resources[LUMBER] + moves[i].in[LUMBER] - moves[i].out[LUMBER] >= 1) && (pos->player[pos->turn].resources[GRAIN] + moves[i].in[GRAIN] - moves[i].out[GRAIN] >= 1) && (pos->player[pos->turn].resources[WOOL] + moves[i].in[WOOL] - moves[i].out[WOOL] >= 1))
				continue;

			if (pos->player[pos->turn].settlement && (pos->player[pos->turn].resources[ORE] + moves[i].in[ORE] - moves[i].out[ORE] >= 3) && (pos->player[pos->turn].resources[GRAIN] + moves[i].in[GRAIN] - moves[i].out[GRAIN] >= 2))
				continue;

			memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
			n--;
			i--;
		}
	}

	size_t j;
	if (type == DICE) {
		j = uniformint(seed, 0, 6) + uniformint(seed, 0, 6);
	}
	else if (type == STEALRANDOM) {
		struct player *them = &pos->player[1 - pos->turn];
		int sum = them->resources[BRICK] + them->resources[LUMBER] + them->resources[GRAIN] + them->resources[ORE] + them->resources[WOOL];
		if (!sum) {
			j = DESERT;
		}
		else {
			int i = uniformint(seed, 0, sum);
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
	else if (type == CHOICE) {
		j = uniformint(seed, 0, n);
	}

	do_move(pos, &moves[j]);

	result = rollout(pos, seed, ply + 1);

	undo_move(pos, &moves[j]);

	if (moves[j].flip)
		result = -result;

	free(moves);

	return result;
}

int mcts_internal(struct position *pos, uint64_t *seed, struct node *node, int ply) {
	int result;
	if ((result = gameover(pos))) {
	}
	else if (node->n == 0) {
		node->type = generate(pos, &node->moves, &node->nmoves);

		node->nodes = calloc(node->nmoves, sizeof(*node->nodes));

		result = rollout(pos, seed, 0);
	}
	else {
		size_t j = 0;
		if (node->type == DICE) {
			j = uniformint(seed, 0, 6) + uniformint(seed, 0, 6);
		}
		else if (node->type == STEALRANDOM) {
			struct player *them = &pos->player[1 - pos->turn];
			int sum = them->resources[BRICK] + them->resources[LUMBER] + them->resources[GRAIN] + them->resources[ORE] + them->resources[WOOL];
			if (!sum) {
				j = DESERT;
			}
			else {
				int i = uniformint(seed, 0, sum);
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
		else if (node->type == CHOICE) {
			double value;
			for (size_t i = 0; i < node->nmoves; i++) {
				int n = node->nodes[i].n;
				int w = node->nodes[i].w;
				int N = node->n;

				double q = (double)w / (2 * n);
				if (node->moves[i].flip)
					q = 1.0 - q;
				double value1 = n == 0 ? INFINITY : q + c * sqrt(log(N) / n);
				if (i == 0 || value1 > value) {
					j = i;
					value = value1;
				}
			}
		}
		do_move(pos, &node->moves[j]);

		result = mcts_internal(pos, seed, &node->nodes[j], ply + 1);

		/* The child's result is from the other player's point of view. */
		if (node->moves[j].flip)
			result = -result;

		undo_move(pos, &node->moves[j]);
	}

	node->n++;

	if (result > 0)
		node->w += 2;
	if (result == 0)
		node->w++;

	return result;
}

void mcts(struct position *pos, int maxtime, struct node **root, uint64_t *seed, struct move *return_move) {
	int64_t starttime = time_now();
	int64_t endtime = starttime + maxtime * TPPERMS;
	int64_t lastinfotime = starttime;

	/* Always run at least once so the root has moves to choose from. */
	int64_t now;
	do {
		mcts_internal(pos, seed, *root, 0);

		now = time_now();
		if (now - lastinfotime > 500 * TPPERMS) {
			lastinfotime = now;
			//int bn = best_node(*root);
		}
	} while (now < endtime);

	/* Insertion sort moves. */
	for (size_t i = 1; i < (*root)->nmoves; i++) {
		struct node node;
		struct move move;
		int key = (*root)->nodes[i].n;
		memcpy(&node, &(*root)->nodes[i], sizeof(node));
		memcpy(&move, &(*root)->moves[i], sizeof(move));

		ssize_t j = i - 1;
		while (j >= 0 && (*root)->nodes[j].n < key) {
		memcpy(&(*root)->nodes[j + 1], &(*root)->nodes[j], sizeof(node));
		memcpy(&(*root)->moves[j + 1], &(*root)->moves[j], sizeof(move));
			j--;
		}

		memcpy(&(*root)->nodes[j + 1], &node, sizeof(node));
		memcpy(&(*root)->moves[j + 1], &move, sizeof(move));
	}

	for (size_t i = 0; i < (*root)->nmoves; i++) {
		struct node *node = &(*root)->nodes[i];
		struct move *move = &(*root)->moves[i];

		/* Win rate for the player choosing at the root. */
		double q = (double)node->w / (2 * node->n);
		if (move->flip)
			q = 1.0 - q;

		print_move(pos, move);
		printf(" (%lf, %d)\n", q, node->n);
		break;
	}

	int bn = best_node(*root);

	memcpy(return_move, &(*root)->moves[bn], sizeof(*return_move));

	struct node *ret = free_node(*root, bn);
	free(*root);
	*root = ret;
}
