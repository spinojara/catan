#include <stdint.h>
#include <math.h>

#define TPPERSEC 1000000000l
#define TPPERMS     1000000l

double c = 1.41;

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

/* Almost every rollout ends the game long before this. It is only a safety
 * net, since a blocked board can stop both players from reaching 8 points.
 */
#define MAX_ROLLOUT_PLIES 1000

int score_tile(int number) {
	return number ? 6 - abs(7 - number) : 0;
}

int production(const struct smtile *smtile) {
	int sum = 0;
	for (int t = 0; smtile->tiles[t]; t++)
		sum += score_tile(smtile->tiles[t]->number);
	return sum;
}

int reaches_free_spot(const struct rtile *rtile) {
	for (int s = 0; rtile->smtiles[s]; s++)
		if (!rtile->smtiles[s]->blocked && !rtile->smtiles[s]->settlement && !rtile->smtiles[s]->city)
			return 1;
	return 0;
}

int rollout(struct position *pos, uint64_t *seed, int ply) {
	if (ply >= MAX_ROLLOUT_PLIES)
		return points(pos, pos->turn) - points(pos, 1 - pos->turn);
	int result;
	if ((result = gameover(pos)))
		return result;

	struct move *moves = NULL;
	size_t n = 0;

	int type = generate(pos, &moves, &n);

	if (type == CHOICE) {
		/* In the setup, only settle on the most productive spots. Random
		 * spots often produce so little that the game takes thousands of
		 * plies.
		 */
		if (pos->setup) {
			int best = 0;
			for (size_t i = 0; i < n; i++)
				best = max(best, production(moves[i].smtile));

			for (size_t i = 0; i < n; i++) {
				if (production(moves[i].smtile) < best - 2) {
					memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
					n--;
					i--;
				}
			}
		}

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

		/* If a road can reach a free spot, only build those roads. */
		int road_reaches = 0;
		for (size_t i = 0; i < n && !road_reaches; i++)
			if (moves[i].type == BUILD && moves[i].building == ROAD && reaches_free_spot(moves[i].rtile))
				road_reaches = 1;

		if (road_reaches) {
			for (size_t i = 0; i < n; i++) {
				if (moves[i].type == BUILD && moves[i].building == ROAD && !reaches_free_spot(moves[i].rtile)) {
					memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
					n--;
					i--;
				}
			}
		}

		/* Only allow trades which bring a build that is not possible
		 * yet closer, such that the cards left over can still be traded
		 * for the rest of it, and which only take cards we are missing.
		 * Anything else throws away 3 cards for nothing.
		 */
		const struct player *player = &pos->player[pos->turn];
		int can_build_road = 0;
		if (!has_reachable && player->road < 15)
			for (int r = 0; r < MAX_ROADS && !can_build_road; r++)
				if (!pos->rtiles[r].road && pos->rtiles[r].reachable[pos->turn])
					can_build_road = 1;

		static const int cost[3][DESERT] = {
			[ROAD] = { [BRICK] = 1, [LUMBER] = 1 },
			[SETTLEMENT] = { [BRICK] = 1, [LUMBER] = 1, [GRAIN] = 1, [WOOL] = 1 },
			[CITY] = { [ORE] = 3, [GRAIN] = 2 },
		};
		const int want[3] = {
			[ROAD] = can_build_road,
			[SETTLEMENT] = has_reachable && player->settlement < 5,
			[CITY] = player->settlement && player->city < 4,
		};

		for (size_t i = 0; i < n; i++) {
			if (moves[i].type != TRADE)
				continue;

			int keep = 0;
			for (int b = ROAD; b <= CITY && !keep; b++) {
				if (!want[b])
					continue;

				int missing_before = 0, missing_after = 0, too_much = 0, trades_left = 0;
				for (int r = 0; r < DESERT; r++) {
					int before = player->resources[r];
					int after = before + moves[i].in[r] - moves[i].out[r];
					int missing = max(0, cost[b][r] - before);
					missing_before += missing;
					missing_after += max(0, cost[b][r] - after);
					too_much |= moves[i].in[r] > missing;
					trades_left += max(0, after - cost[b][r]) / 4;
				}

				keep = missing_after < missing_before && missing_after <= trades_left && !too_much;
			}

			if (!keep) {
				memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
				n--;
				i--;
			}
		}

		/* Remove any robber moves which don't attack the enemy,
		 * and those which attacks us.
		 */
		int would_filter_all = 1;
		for (int j = 0; j < 2; j++) {
			for (size_t i = 0; i < n; i++) {
				if (moves[i].type != ROBBER)
					continue;

				int hurts_us = 0, hurts_them = 0;
				for (int s = 0; moves[i].tile->smtiles[s]; s++) {
					if ((moves[i].tile->smtiles[s]->city || moves[i].tile->smtiles[s]->settlement) && moves[i].tile->smtiles[s]->player == pos->turn)
						hurts_us = 1;
					if ((moves[i].tile->smtiles[s]->city || moves[i].tile->smtiles[s]->settlement) && moves[i].tile->smtiles[s]->player == 1 - pos->turn)
						hurts_them = 1;
				}

				int keep = !hurts_us && hurts_them;
				int definitely_dont_keep = hurts_us && !hurts_them;

				/* Check if we can filter without filtering everything. */
				if (j == 0) {
					if (keep) {
						would_filter_all = 0;
						break;
					}
				}
				/* Actually filter. */
				else {
					if ((!would_filter_all && !keep) || definitely_dont_keep) {
						memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
						n--;
						i--;
					}
				}
			}
		}

		/* Filter buying development cards. */
		for (size_t i = 0; i < n; i++) {
			if (moves[i].type == BUILD && moves[i].building == DEVELOPMENT) {
				memcpy(&moves[i], &moves[n - 1], sizeof(moves[i]));
				n--;
				i--;
			}
		}
	}

	size_t j;
	if (type == DICE) {
		j = uniformint(seed, 0, 6) + uniformint(seed, 0, 6);
	}
	else if (type == STEALRANDOM) {
		struct player *them = &pos->player[1 - pos->turn];
		int sum = them->resources[BRICK] + them->resources[LUMBER] + them->resources[GRAIN] + them->resources[ORE] + them->resources[WOOL];
		if (!sum || !can_steal(pos)) {
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
	else if (type == DRAWRANDOM) {
		int sum = pos->development[KNIGHT] + pos->development[ROADBUILDING] + pos->development[YEAROFPLENTY] + pos->development[MONOPOLY] + pos->development[VICTORYPOINT];
		int i = uniformint(seed, 0, sum);

		if (i < pos->development[KNIGHT])
			j = KNIGHT;
		else if (i < pos->development[KNIGHT] + pos->development[ROADBUILDING])
			j = ROADBUILDING;
		else if (i < pos->development[KNIGHT] + pos->development[ROADBUILDING] + pos->development[YEAROFPLENTY])
			j = YEAROFPLENTY;
		else if (i < pos->development[KNIGHT] + pos->development[ROADBUILDING] + pos->development[YEAROFPLENTY] + pos->development[MONOPOLY])
			j = MONOPOLY;
		else
			j = VICTORYPOINT;
	}
	else if (type == CHOICE) {
		j = uniformint(seed, 0, n);
	}
	else {
		exit(67);
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
		/* add_move leaves room to grow, but nothing is ever added to a
		 * node's moves, so give it back.
		 */
		if (node->nmoves)
			node->moves = realloc(node->moves, node->nmoves * sizeof(*node->moves));

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
			if (!sum || !can_steal(pos)) {
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
		else if (node->type == DRAWRANDOM) {
			int sum = pos->development[KNIGHT] + pos->development[ROADBUILDING] + pos->development[YEAROFPLENTY] + pos->development[MONOPOLY] + pos->development[VICTORYPOINT];
			int i = uniformint(seed, 0, sum);

			if (i < pos->development[KNIGHT])
				j = KNIGHT;
			else if (i < pos->development[KNIGHT] + pos->development[ROADBUILDING])
				j = ROADBUILDING;
			else if (i < pos->development[KNIGHT] + pos->development[ROADBUILDING] + pos->development[YEAROFPLENTY])
				j = YEAROFPLENTY;
			else if (i < pos->development[KNIGHT] + pos->development[ROADBUILDING] + pos->development[YEAROFPLENTY] + pos->development[MONOPOLY])
				j = MONOPOLY;
			else
				j = VICTORYPOINT;
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

/* How the chosen move is rated is printed to info. */
void mcts(struct position *pos, int maxtime, struct node **root, uint64_t *seed, struct move *return_move, FILE *info) {
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

		char str[256];
		move_to_str(move, str);
		fprintf(info, "move %s (%lf, %d)\n", str, q, node->n);
		break;
	}

	int bn = best_node(*root);

	memcpy(return_move, &(*root)->moves[bn], sizeof(*return_move));

	struct node *ret = free_node(*root, bn);
	free(*root);
	*root = ret;
}
