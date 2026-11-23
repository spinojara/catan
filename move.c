enum {
	BUILD = 0x1,
	TRADE = 0x2,
	ENDTURN = 0x4,
	RANDOM = 0x8,
	SETUP = 0x10,
	DISCARD = 0x20,
	STEAL = 0x40,
	ROBBER = 0x80,
	DRAW = 0x100,
	PLAYKNIGHT = 0x200,
	PLAYROADBUILDING = 0x400,
	PLAYYEAROFPLENTY = 0x800,
	PLAYMONOPOLY = 0x1000
};

enum {
	CHOICE,
	STEALRANDOM,
	DICE,
	DRAWRANDOM,
};

enum {
	ROAD,
	SETTLEMENT,
	CITY,
	DEVELOPMENT,
};

/* This allows doing many trades in a single move which
 * should make the search more efficient. We should however
 * still allow trading many times in a row. This could potentially
 * get rid of more cards intentionally and could possibly be favorable.
 */
struct move {
	/* Flip turn. */
	int flip;

	int type;

	/* Trade, discard or stealing cards. */
	int out[DESERT];
	int in[DESERT];

	/* Build. */
	int building;

	/* If building == SETTLEMENT then this might make a road unreachable. */
	int unreachable[3];

	struct smtile *smtile;
	struct rtile *rtile;
	struct rtile *rtile1;

	/* Robber. */
	struct tile *tile;
	struct tile *prevtile;

	/* Random. */
	int number;

	/* Old longest road. */
	int prevroad[2];
	int prevlongestroad;
	int prevarmy;

	/* Draw a development card. */
	int development;

	/* End turn, so that undo can restore what was bought and played. */
	int bought[ALLCARDS];
	int played;
};

/* Add a move to the list, growing it in chunks instead of one move at a
 * time. A list starts as NULL with n = 0. It then always has room for 16
 * moves, or the next power of two, so the size never needs to be stored.
 */
struct move *add_move(struct move **moves, size_t *n) {
	if (*n == 0 || (*n >= 16 && !(*n & (*n - 1)))) {
		*moves = realloc(*moves, (*n ? 2 * *n : 16) * sizeof(**moves));
		if (!*moves) {
			fprintf(stderr, "error: realloc\n");
			exit(1);
		}
	}
	return &(*moves)[(*n)++];
}

void generate_setup(struct position *pos, struct move **moves, size_t *n) {
	int flip = (pos->setup % 2) ? 1 : 0;

	for (int s = 0; s < MAX_SETTLEMENTS; s++) {
		if (pos->smtiles[s].blocked || pos->smtiles[s].settlement || pos->smtiles[s].city)
			continue;

		/* Nothing can have been built on this rtile. */
		for (int r = 0; pos->smtiles[s].rtiles[r]; r++) {
			struct move *move = add_move(moves, n);
			move->type = SETUP;
			move->smtile = &pos->smtiles[s];
			move->rtile = pos->smtiles[s].rtiles[r];
			move->flip = flip;
			/* No opponent road can touch a spot that is free in the setup. */
			memset(move->unreachable, 0, sizeof(move->unreachable));
		}
	}
}

void generate_trade(const struct position *pos, struct move **moves, size_t *n) {
	const struct player *player = &pos->player[pos->turn];

	for (int resource = 0; resource < DESERT; resource++) {
		if (player->resources[resource] >= 4) {
			for (int r2 = 0; r2 < DESERT; r2++) {
				/* Could possibly want to trade 4 for 1 if
				 * we want to reduce the number of cards.
				 * Let's allow it.
				 */
				struct move *move = add_move(moves, n);
				memset(move->out, 0, sizeof(move->out));
				memset(move->in, 0, sizeof(move->in));

				move->out[resource] = 4;

				move->in[r2] = 1;

				move->type = TRADE;
				move->flip = 0;
			}
		}

		if (player->resources[resource] >= 3 && player->generalharbor) {
			for (int r2 = 0; r2 < DESERT; r2++) {
				struct move *move = add_move(moves, n);
				memset(move->out, 0, sizeof(move->out));
				memset(move->in, 0, sizeof(move->in));

				move->out[resource] = 3;

				move->in[r2] = 1;

				move->type = TRADE;
				move->flip = 0;
			}
		}

		if (player->resources[resource] >= 2 && player->harbor[resource]) {
			for (int r2 = 0; r2 < DESERT; r2++) {
				struct move *move = add_move(moves, n);
				memset(move->out, 0, sizeof(move->out));
				memset(move->in, 0, sizeof(move->in));

				move->out[resource] = 2;

				move->in[r2] = 1;

				move->type = TRADE;
				move->flip = 0;
			}
		}
	}
}

void generate_build(struct position *pos, struct move **moves, size_t *n) {
	const struct player *player = &pos->player[pos->turn];

	if (player->resources[ORE] >= 3 && player->resources[GRAIN] >= 2 && player->city < 4) {
		for (int s = 0; s < MAX_SETTLEMENTS; s++) {
			struct smtile *smtile = &pos->smtiles[s];
			if (smtile->player == pos->turn && smtile->settlement) {
				struct move *move = add_move(moves, n);
				move->building = CITY;
				move->smtile = smtile;
				move->type = BUILD;
				move->flip = 0;
			}
		}
	}

	if (player->resources[WOOL] >= 1 && player->resources[GRAIN] >= 1 && player->resources[LUMBER] >= 1 && player->resources[BRICK] >= 1 && player->settlement < 5) {
		for (int s = 0; s < MAX_SETTLEMENTS; s++) {
			struct smtile *smtile = &pos->smtiles[s];
			if (!smtile->city && !smtile->settlement && !smtile->blocked && smtile->reachable[pos->turn]) {
				struct move *move = add_move(moves, n);
				move->building = SETTLEMENT;
				move->smtile = smtile;
				move->type = BUILD;
				move->flip = 0;

				memset(move->unreachable, 0, sizeof(move->unreachable));

				/* Make sure new settlements make roads unreachable for the other player. */
				for (int r1 = 0; smtile->rtiles[r1]; r1++) {
					for (int r2 = 0; smtile->rtiles[r2]; r2++) {
						if (r1 == r2)
							continue;

						if (smtile->rtiles[r1]->road && smtile->rtiles[r1]->player == 1 - pos->turn)
							move->unreachable[r2] += 1;
					}
				}
			}
		}
	}

	if (player->resources[LUMBER] >= 1 && player->resources[BRICK] >= 1 && player->road < 15) {
		for (int r = 0; r < MAX_ROADS; r++) {
			struct rtile *rtile = &pos->rtiles[r];
			if (!rtile->road && rtile->reachable[pos->turn]) {
				struct move *move = add_move(moves, n);
				move->building = ROAD;
				move->rtile = rtile;
				move->type = BUILD;
				move->flip = 0;
			}
		}
	}

	if (player->resources[ORE] >= 1 && player->resources[GRAIN] >= 1 && player->resources[WOOL] >= 1 && (pos->development[KNIGHT] || pos->development[ROADBUILDING] || pos->development[YEAROFPLENTY] || pos->development[MONOPOLY] || pos->development[VICTORYPOINT])) {
		struct move *move = add_move(moves, n);
		move->building = DEVELOPMENT;
		move->type = BUILD;
		move->flip = 0;
	}
}

void generate_play(struct position *pos, struct move **moves, size_t *n) {
	/* Only one development card can be played each turn, and not one
	 * that was bought this turn.
	 */
	if (pos->played)
		return;

	if (pos->player[pos->turn].development[KNIGHT] > pos->bought[KNIGHT]) {
		struct move *move = add_move(moves, n);
		move->flip = 0;
		move->type = PLAYKNIGHT;
	}

	if (pos->player[pos->turn].development[YEAROFPLENTY] > pos->bought[YEAROFPLENTY]) {
		for (int r1 = 0; r1 < DESERT; r1++) {
			for (int r2 = r1; r2 < DESERT; r2++) {
				struct move *move = add_move(moves, n);
				move->flip = 0;
				move->type = PLAYYEAROFPLENTY;
				memset(move->in, 0, sizeof(move->in));
				move->in[r1]++;
				move->in[r2]++;
			}
		}
	}

	if (pos->player[pos->turn].development[ROADBUILDING] > pos->bought[ROADBUILDING] && pos->player[pos->turn].road <= 13) {
		for (int r = 0; r < MAX_ROADS; r++) {
			struct rtile *rtile = &pos->rtiles[r];
			if (!rtile->road && rtile->reachable[pos->turn]) {
				/* Make the move */
				rtile->road = 1;
				for (int i = 0; rtile->rtiles[i]; i++) {
					struct smtile *blocking_smtile = pos->common_smtile[rtile->index][rtile->rtiles[i]->index];

					if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
						rtile->rtiles[i]->reachable[pos->turn]++;
				}
				for (int i = 0; rtile->smtiles[i]; i++)
					rtile->smtiles[i]->reachable[pos->turn]++;

				/* Check. */
				for (int r1 = 0; r1 < MAX_ROADS; r1++) {
					struct rtile *rtile1 = &pos->rtiles[r1];
					if (!rtile1->road && rtile1->reachable[pos->turn]) {

						/* Dedup by ordering. */
						int already_has = 0;
						for (size_t i = 0; i < *n && !already_has; i++) {
							if ((*moves)[i].type == PLAYROADBUILDING && (*moves)[i].rtile == rtile1 && (*moves)[i].rtile1 == rtile) {
								already_has = 1;
							}
						}
						if (already_has)
							continue;

						struct move *move = add_move(moves, n);
						move->flip = 0;
						move->type = PLAYROADBUILDING;
						move->rtile = rtile;
						move->rtile1 = rtile1;
					}
				}

				/* Unmake the move */
				rtile->road = 0;
				for (int i = 0; rtile->rtiles[i]; i++) {
					struct smtile *blocking_smtile = pos->common_smtile[rtile->index][rtile->rtiles[i]->index];

					if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
						rtile->rtiles[i]->reachable[pos->turn]--;
				}
				for (int i = 0; rtile->smtiles[i]; i++)
					rtile->smtiles[i]->reachable[pos->turn]--;

			}
		}
	}

	if (pos->player[pos->turn].development[MONOPOLY] > pos->bought[MONOPOLY]) {
		for (int r = 0; r < DESERT; r++) {
			struct move *move = add_move(moves, n);
			move->flip = 0;
			move->type = PLAYMONOPOLY;
			move->number = r;
		}
	}
}

void do_knight(struct position *pos, struct move *move) {
	(void)move;
	pos->player[pos->turn].development[KNIGHT]--;
	pos->played = 1;
	pos->player[pos->turn].knights++;

	if (pos->player[pos->turn].knights >= 3 && pos->player[pos->turn].knights > pos->player[1 - pos->turn].knights)
		pos->biggestarmy = pos->turn;

	/* We are now in the same situation as if we rolled a 7. */
	pos->seven = 2;
}

void do_yearofplenty(struct position *pos, struct move *move) {
	pos->player[pos->turn].development[YEAROFPLENTY]--;
	pos->played = 1;

	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] += move->in[r];
}

void do_roadbuilding(struct position *pos, struct move *move) {
	pos->player[pos->turn].development[ROADBUILDING]--;
	pos->played = 1;

	move->rtile->road = 1;
	move->rtile->player = pos->turn;

	/* A road is only reachable if it was not blocked by a settlement/city. */
	for (int i = 0; move->rtile->rtiles[i]; i++) {
		struct smtile *blocking_smtile = pos->common_smtile[move->rtile->index][move->rtile->rtiles[i]->index];

		if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
			move->rtile->rtiles[i]->reachable[pos->turn]++;
	}
	for (int i = 0; move->rtile->smtiles[i]; i++)
		move->rtile->smtiles[i]->reachable[pos->turn]++;

	move->rtile1->road = 1;
	move->rtile1->player = pos->turn;

	/* A road is only reachable if it was not blocked by a settlement/city. */
	for (int i = 0; move->rtile1->rtiles[i]; i++) {
		struct smtile *blocking_smtile = pos->common_smtile[move->rtile1->index][move->rtile1->rtiles[i]->index];

		if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
			move->rtile1->rtiles[i]->reachable[pos->turn]++;
	}
	for (int i = 0; move->rtile1->smtiles[i]; i++)
		move->rtile1->smtiles[i]->reachable[pos->turn]++;

	pos->player[pos->turn].road += 2;

	pos->player[pos->turn].longestroad = compute_longest_road(pos, pos->turn);
}

void do_monopoly(struct position *pos, struct move *move) {
	pos->player[pos->turn].development[MONOPOLY]--;
	pos->played = 1;

	move->in[move->number] = pos->player[1 - pos->turn].resources[move->number];
	pos->player[pos->turn].resources[move->number] += pos->player[1 - pos->turn].resources[move->number];
	pos->player[1 - pos->turn].resources[move->number] = 0;
}

void undo_knight(struct position *pos, struct move *move) {
	(void)move;
	pos->player[pos->turn].development[KNIGHT]++;
	pos->played = 0;
	pos->player[pos->turn].knights--;

	pos->seven = 0;
}

void undo_yearofplenty(struct position *pos, struct move *move) {
	pos->player[pos->turn].development[YEAROFPLENTY]++;
	pos->played = 0;

	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] -= move->in[r];
}

void undo_roadbuilding(struct position *pos, struct move *move) {
	pos->player[pos->turn].development[ROADBUILDING]++;
	pos->played = 0;

	move->rtile->road = 0;

	/* A road is only reachable if it was not blocked by a settlement/city. */
	for (int i = 0; move->rtile->rtiles[i]; i++) {
		struct smtile *blocking_smtile = pos->common_smtile[move->rtile->index][move->rtile->rtiles[i]->index];

		if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
			move->rtile->rtiles[i]->reachable[pos->turn]--;
	}
	for (int i = 0; move->rtile->smtiles[i]; i++)
		move->rtile->smtiles[i]->reachable[pos->turn]--;

	move->rtile1->road = 0;

	/* A road is only reachable if it was not blocked by a settlement/city. */
	for (int i = 0; move->rtile1->rtiles[i]; i++) {
		struct smtile *blocking_smtile = pos->common_smtile[move->rtile1->index][move->rtile1->rtiles[i]->index];

		if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
			move->rtile1->rtiles[i]->reachable[pos->turn]--;
	}
	for (int i = 0; move->rtile1->smtiles[i]; i++)
		move->rtile1->smtiles[i]->reachable[pos->turn]--;

	pos->player[pos->turn].road -= 2;
}

void undo_monopoly(struct position *pos, struct move *move) {
	pos->player[pos->turn].development[MONOPOLY]++;
	pos->played = 0;

	pos->player[pos->turn].resources[move->number] -= move->in[move->number];
	pos->player[1 - pos->turn].resources[move->number] = move->in[move->number];
}

void do_build(struct position *pos, struct move *move) {
	if (move->building == DEVELOPMENT) {
		pos->drawcard = 1;

		pos->player[pos->turn].resources[ORE]--;
		pos->player[pos->turn].resources[GRAIN]--;
		pos->player[pos->turn].resources[WOOL]--;
	}
	else if (move->building == ROAD) {
		move->rtile->road = 1;
		move->rtile->player = pos->turn;

		/* A road is only reachable if it was not blocked by a settlement/city. */
		for (int i = 0; move->rtile->rtiles[i]; i++) {
			struct smtile *blocking_smtile = pos->common_smtile[move->rtile->index][move->rtile->rtiles[i]->index];

			if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
				move->rtile->rtiles[i]->reachable[pos->turn]++;
		}
		for (int i = 0; move->rtile->smtiles[i]; i++)
			move->rtile->smtiles[i]->reachable[pos->turn]++;

		pos->player[pos->turn].resources[BRICK]--;
		pos->player[pos->turn].resources[LUMBER]--;

		pos->player[pos->turn].road++;

		pos->player[pos->turn].longestroad = compute_longest_road(pos, pos->turn);
	}
	else if (move->building == CITY || move->building == SETTLEMENT) {
		pos->player[pos->turn].score++;

		move->smtile->settlement = move->building == SETTLEMENT;
		move->smtile->city = move->building == CITY;
		move->smtile->player = pos->turn;

		for (int i = 0; move->smtile->rtiles[i]; i++)
			for (int j = 0; move->smtile->rtiles[i]->smtiles[j]; j++)
				move->smtile->rtiles[i]->smtiles[j]->blocked++;

		if (move->building == CITY) {
			pos->player[pos->turn].resources[ORE] -= 3;
			pos->player[pos->turn].resources[GRAIN] -= 2;
		}
		else if (move->building == SETTLEMENT) {
			pos->player[pos->turn].resources[LUMBER]--;
			pos->player[pos->turn].resources[BRICK]--;
			pos->player[pos->turn].resources[WOOL]--;
			pos->player[pos->turn].resources[GRAIN]--;

			for (int r = 0; move->smtile->rtiles[r]; r++)
				move->smtile->rtiles[r]->reachable[1 - pos->turn] -= move->unreachable[r];

			/* Maybe enemy road got cut off. */
			pos->player[1 - pos->turn].longestroad = compute_longest_road(pos, 1 - pos->turn);

			if (move->smtile->harbor != -1) {
				/* This is a general 3:1 harbor. */
				if (move->smtile->harbor == DESERT)
					pos->player[pos->turn].generalharbor++;
				else
					pos->player[pos->turn].harbor[move->smtile->harbor]++;
			}
		}

		pos->player[pos->turn].settlement += (move->building == SETTLEMENT) - (move->building == CITY);
		pos->player[pos->turn].city += move->building == CITY;
	}
}

void undo_build(struct position *pos, struct move *move) {
	if (move->building == DEVELOPMENT) {
		pos->drawcard = 0;

		pos->player[pos->turn].resources[ORE]++;
		pos->player[pos->turn].resources[GRAIN]++;
		pos->player[pos->turn].resources[WOOL]++;
	}
	else if (move->building == ROAD) {
		move->rtile->road = 0;

		/* A road is only reachable if it was not blocked by a settlement/city. */
		for (int i = 0; move->rtile->rtiles[i]; i++) {
			struct smtile *blocking_smtile = pos->common_smtile[move->rtile->index][move->rtile->rtiles[i]->index];

			if (!(blocking_smtile->player == 1 - pos->turn && (blocking_smtile->city || blocking_smtile->settlement)))
				move->rtile->rtiles[i]->reachable[pos->turn]--;
		}
		for (int i = 0; move->rtile->smtiles[i]; i++)
			move->rtile->smtiles[i]->reachable[pos->turn]--;

		pos->player[pos->turn].resources[BRICK]++;
		pos->player[pos->turn].resources[LUMBER]++;

		pos->player[pos->turn].road--;
	}
	else if (move->building == CITY || move->building == SETTLEMENT) {
		pos->player[pos->turn].score--;

		move->smtile->settlement = move->building == CITY;
		move->smtile->city = 0;
		if (move->building == SETTLEMENT)
			move->smtile->player = 0;

		for (int i = 0; move->smtile->rtiles[i]; i++)
			for (int j = 0; move->smtile->rtiles[i]->smtiles[j]; j++)
				move->smtile->rtiles[i]->smtiles[j]->blocked--;

		if (move->building == CITY) {
			pos->player[pos->turn].resources[ORE] += 3;
			pos->player[pos->turn].resources[GRAIN] += 2;
		}
		else if (move->building == SETTLEMENT) {
			pos->player[pos->turn].resources[LUMBER]++;
			pos->player[pos->turn].resources[BRICK]++;
			pos->player[pos->turn].resources[WOOL]++;
			pos->player[pos->turn].resources[GRAIN]++;

			for (int r = 0; move->smtile->rtiles[r]; r++)
				move->smtile->rtiles[r]->reachable[1 - pos->turn] += move->unreachable[r];

			if (move->smtile->harbor != -1) {
				if (move->smtile->harbor == DESERT)
					pos->player[pos->turn].generalharbor--;
				else
					pos->player[pos->turn].harbor[move->smtile->harbor]--;
			}
		}

		pos->player[pos->turn].settlement -= (move->building == SETTLEMENT) - (move->building == CITY);
		pos->player[pos->turn].city -= move->building == CITY;
	}
}

void do_random(struct position *pos, int n) {
	/* A seven starts the discard and steal phase, see generate. */
	if (n == 7)
		pos->seven = 4;

	for (int t = 0; t < MAX_TILES; t++) {
		struct tile *tile = &pos->tiles[t];
		if (tile->number != n || tile->type == DESERT || tile->robber)
			continue;

		for (int s = 0; tile->smtiles[s]; s++) {
			struct smtile *smtile = tile->smtiles[s];
			if (smtile->city || smtile->settlement)
				pos->player[smtile->player].resources[tile->type] += 1 + smtile->city;
		}
	}

	pos->dice = 0;
}

void undo_random(struct position *pos, int n) {
	if (n == 7)
		pos->seven = 0;

	for (int t = 0; t < MAX_TILES; t++) {
		struct tile *tile = &pos->tiles[t];
		if (tile->number != n || tile->type == DESERT || tile->robber)
			continue;

		for (int s = 0; tile->smtiles[s]; s++) {
			struct smtile *smtile = tile->smtiles[s];
			if (smtile->city || smtile->settlement)
				pos->player[smtile->player].resources[tile->type] -= 1 + smtile->city;
		}
	}

	pos->dice = 1;
}

void do_trade(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] += move->in[r] - move->out[r];
}

void undo_trade(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] -= move->in[r] - move->out[r];
}

/* The robber can only steal from the opponent if they have a settlement
 * or city next to the tile it was moved to.
 */
int can_steal(const struct position *pos) {
	const struct tile *tile = &pos->tiles[pos->robber];
	for (int s = 0; tile->smtiles[s]; s++) {
		const struct smtile *smtile = tile->smtiles[s];
		if (smtile->player == 1 - pos->turn && (smtile->settlement || smtile->city))
			return 1;
	}
	return 0;
}

/* Generate all steals, even if they are not possible. We will just not move into them later. */
void generate_steal(struct position *pos, struct move **moves, size_t *n) {
	(void)pos;
	for (int r = 0; r < DESERT; r++) {
		struct move *move = add_move(moves, n);
		move->type = STEAL;
		memset(move->in, 0, sizeof(move->in));
		move->in[r] = 1;
		move->flip = 0;
	}

	struct move *move = add_move(moves, n);
	move->type = STEAL;
	move->flip = 0;
	memset(move->in, 0, sizeof(move->in));
}

void generate_discard(struct position *pos, struct move **moves, size_t *n) {
	struct player *player = &pos->player[pos->turn];
	int sum = player->resources[BRICK] + player->resources[LUMBER] + player->resources[GRAIN] + player->resources[WOOL] + player->resources[ORE];
	if (sum <= 7) {
		struct move *move = add_move(moves, n);
		move->type = DISCARD;
		memset(move->out, 0, sizeof(move->out));
		move->flip = 1;
	}
	else {
		int discard = sum / 2;

		for (int tb = 0; tb <= min(discard, player->resources[BRICK]); tb++) {
		for (int tl = 0; tl <= min(discard - tb, player->resources[LUMBER]); tl++) {
		for (int tg = 0; tg <= min(discard - tb - tl, player->resources[GRAIN]); tg++) {
		for (int tw = 0; tw <= min(discard - tb - tl - tg, player->resources[WOOL]); tw++) {
			int to = discard - tb - tl - tg - tw;
			if (to > player->resources[ORE])
				continue;

			struct move *move = add_move(moves, n);
			move->type = DISCARD;
			move->out[BRICK] = tb;
			move->out[LUMBER] = tl;
			move->out[GRAIN] = tg;
			move->out[WOOL] = tw;
			move->out[ORE] = to;
			move->flip = 1;
		}
		}
		}
		}
	}
}

void generate_robber(struct position *pos, struct move **moves, size_t *n) {
	for (int t = 0; t < MAX_TILES; t++) {
		if (pos->tiles[t].robber)
			continue;
		struct move *move = add_move(moves, n);
		move->type = ROBBER;
		move->tile = &pos->tiles[t];
		move->flip = 0;
	}
}

void do_robber(struct position *pos, struct move *move) {
	move->prevtile = &pos->tiles[pos->robber];

	pos->tiles[pos->robber].robber = 0;
	move->tile->robber = 1;
	pos->robber = move->tile->index;

	pos->seven--;
}

void undo_robber(struct position *pos, struct move *move) {
	move->tile->robber = 0;
	move->prevtile->robber = 1;
	pos->robber = move->prevtile->index;

	pos->seven++;
}

void do_draw(struct position *pos, struct move *move) {
	pos->drawcard = 0;

	pos->bought[move->development]++;
	pos->player[pos->turn].development[move->development]++;
	pos->development[move->development]--;

	if (move->development == VICTORYPOINT)
		pos->player[pos->turn].score++;
}

void undo_draw(struct position *pos, struct move *move) {
	pos->drawcard = 1;

	pos->bought[move->development]--;
	pos->player[pos->turn].development[move->development]--;
	pos->development[move->development]++;

	if (move->development == VICTORYPOINT)
		pos->player[pos->turn].score--;
}

/* The next player can play everything, and has not played anything yet. */
void do_endturn(struct position *pos, struct move *move) {
	memcpy(move->bought, pos->bought, sizeof(move->bought));
	memset(pos->bought, 0, sizeof(pos->bought));
	move->played = pos->played;
	pos->played = 0;
}

void undo_endturn(struct position *pos, struct move *move) {
	memcpy(pos->bought, move->bought, sizeof(pos->bought));
	pos->played = move->played;
}

int generate(struct position *pos, struct move **moves, size_t *n) {
	if (pos->dice) {
		for (int number = 2; number <= 12; number++) {
			struct move *move = add_move(moves, n);
			move->type = RANDOM;
			move->number = number;
			move->flip = 0;
		}
		return DICE;
	}
	else if (pos->drawcard) {
		for (int card = KNIGHT; card < ALLCARDS; card++) {
			struct move *move = add_move(moves, n);
			move->type = DRAW;
			move->development = card;
			move->flip = 0;
		}
		return DRAWRANDOM;
	}
	else {
		if (pos->seven) {
			switch (pos->seven) {
			case 4:
			case 3:
				generate_discard(pos, moves, n);
				return CHOICE;
			case 2:
				generate_robber(pos, moves, n);
				return CHOICE;
			case 1:
				generate_steal(pos, moves, n);
				return STEALRANDOM;
			}
		}
		else if (pos->setup) {
			generate_setup(pos, moves, n);
		}
		else {
			struct move *move = add_move(moves, n);
			move->type = ENDTURN;
			move->flip = 1;
			generate_build(pos, moves, n);
			generate_trade(pos, moves, n);
			generate_play(pos, moves, n);
		}
		return CHOICE;
	}
}

void do_setup(struct position *pos, struct move *move) {
	move->building = ROAD;
	do_build(pos, move);
	move->building = SETTLEMENT;
	do_build(pos, move);

	pos->player[pos->turn].resources[BRICK] += 2;
	pos->player[pos->turn].resources[LUMBER] += 2;
	pos->player[pos->turn].resources[WOOL]++;
	pos->player[pos->turn].resources[GRAIN]++;

	if (pos->setup >= 3) {
		for (int t = 0; move->smtile->tiles[t]; t++)
			if (move->smtile->tiles[t]->type != DESERT)
				pos->player[pos->turn].resources[move->smtile->tiles[t]->type]++;
	}

	pos->setup = (pos->setup + 1) % 5;

	if (pos->setup == 0)
		pos->dice = 1;
}

void undo_setup(struct position *pos, struct move *move) {
	move->building = ROAD;
	undo_build(pos, move);
	move->building = SETTLEMENT;
	undo_build(pos, move);

	pos->player[pos->turn].resources[BRICK] -= 2;
	pos->player[pos->turn].resources[LUMBER] -= 2;
	pos->player[pos->turn].resources[WOOL]--;
	pos->player[pos->turn].resources[GRAIN]--;

	/* Take back the first dice roll that ending the setup queued up. */
	if (pos->setup == 0)
		pos->dice = 0;

	pos->setup = (pos->setup + 4) % 5;

	if (pos->setup >= 3) {
		for (int t = 0; move->smtile->tiles[t]; t++)
			if (move->smtile->tiles[t]->type != DESERT)
				pos->player[pos->turn].resources[move->smtile->tiles[t]->type]--;
	}
}

void do_discard(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] -= move->out[r];

	pos->seven--;
}

void undo_discard(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] += move->out[r];

	pos->seven++;
}

void do_steal(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++) {
		pos->player[pos->turn].resources[r] += move->in[r];
		pos->player[1 - pos->turn].resources[r] -= move->in[r];
	}

	pos->seven--;
}

void undo_steal(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++) {
		pos->player[pos->turn].resources[r] -= move->in[r];
		pos->player[1 - pos->turn].resources[r] += move->in[r];
	}

	pos->seven++;
}

void do_move(struct position *pos, struct move *move) {
	move->prevarmy = pos->biggestarmy;
	move->prevroad[0] = pos->player[0].longestroad;
	move->prevroad[1] = pos->player[1].longestroad;
	move->prevlongestroad = pos->longestroad;

	switch (move->type) {
	case BUILD:
		do_build(pos, move);
		break;
	case TRADE:
		do_trade(pos, move);
		break;
	case SETUP:
		do_setup(pos, move);
		break;
	case DISCARD:
		do_discard(pos, move);
		break;
	case RANDOM:
		do_random(pos, move->number);
		break;
	case STEAL:
		do_steal(pos, move);
		break;
	case ROBBER:
		do_robber(pos, move);
		break;
	case DRAW:
		do_draw(pos, move);
		break;
	case PLAYKNIGHT:
		do_knight(pos, move);
		break;
	case PLAYYEAROFPLENTY:
		do_yearofplenty(pos, move);
		break;
	case PLAYROADBUILDING:
		do_roadbuilding(pos, move);
		break;
	case PLAYMONOPOLY:
		do_monopoly(pos, move);
		break;
	case ENDTURN:
		do_endturn(pos, move);
		break;
	default:
		printf("%d\n", move->type);
		exit(100);
		break;
	}

	if (move->flip)
		pos->turn = 1 - pos->turn;

	if (move->type == ENDTURN)
		pos->dice = 1;

	if (pos->longestroad != -1 && pos->player[pos->longestroad].longestroad < 5)
		pos->longestroad = -1;

	if (pos->player[0].longestroad >= 5 && pos->player[0].longestroad > pos->player[1].longestroad)
		pos->longestroad = 0;

	if (pos->player[1].longestroad >= 5 && pos->player[1].longestroad > pos->player[0].longestroad)
		pos->longestroad = 1;
}

void undo_move(struct position *pos, struct move *move) {
	pos->biggestarmy = move->prevarmy;
	pos->player[0].longestroad = move->prevroad[0];
	pos->player[1].longestroad = move->prevroad[1];
	pos->longestroad = move->prevlongestroad;

	/* Take back the dice roll that do_move queued up. */
	if (move->type == ENDTURN)
		pos->dice = 0;

	if (move->flip)
		pos->turn = 1 - pos->turn;

	switch (move->type) {
	case BUILD:
		undo_build(pos, move);
		break;
	case TRADE:
		undo_trade(pos, move);
		break;
	case SETUP:
		undo_setup(pos, move);
		break;
	case DISCARD:
		undo_discard(pos, move);
		break;
	case RANDOM:
		undo_random(pos, move->number);
		break;
	case STEAL:
		undo_steal(pos, move);
		break;
	case ROBBER:
		undo_robber(pos, move);
		break;
	case DRAW:
		undo_draw(pos, move);
		break;
	case PLAYKNIGHT:
		undo_knight(pos, move);
		break;
	case PLAYYEAROFPLENTY:
		undo_yearofplenty(pos, move);
		break;
	case PLAYROADBUILDING:
		undo_roadbuilding(pos, move);
		break;
	case PLAYMONOPOLY:
		undo_monopoly(pos, move);
		break;
	case ENDTURN:
		undo_endturn(pos, move);
		break;
	default:
		break;
	}
}

void move_to_str(struct move *move, char str[256]) {
	size_t i = 0;
	if (move->type == BUILD) {
		i += sprintf(str + i, "build ");

		if (move->building == ROAD) {
			i += sprintf(str + i, "road %d", move->rtile->index);
		}
		else if (move->building == SETTLEMENT) {
			i += sprintf(str + i, "settlement %d", move->smtile->index);
		}
		else if (move->building == CITY) {
			i += sprintf(str + i, "city %d", move->smtile->index);
		}
		else {
			i += sprintf(str + i, "development card");
		}
	}
	else if (move->type == SETUP) {
		i += sprintf(str + i, "setup settlement %d road %d", move->smtile->index, move->rtile->index);
	}
	else if (move->type == TRADE) {
		i += sprintf(str + i, "trade ");

		for (int r = 0; r < DESERT; r++) {
			if (move->out[r])
				i += sprintf(str + i, "%d %s ", move->out[r], resource_to_string(r));
		}
		i += sprintf(str + i, "->");
		for (int r = 0; r < DESERT; r++) {
			if (move->in[r])
				i += sprintf(str + i, " %d %s", move->in[r], resource_to_string(r));
		}
	}
	else if (move->type == ENDTURN) {
		i += sprintf(str + i, "end turn");
	}
	else if (move->type == RANDOM) {
		i += sprintf(str + i, "roll %d", move->number);
	}
	else if (move->type == DISCARD) {
		i += sprintf(str + i, "discard");
		int discarded = 0;
		for (int r = 0; r < DESERT; r++) {
			if (move->out[r]) {
				discarded = 1;
				i += sprintf(str + i, " %d %s", move->out[r], resource_to_string(r));
			}
		}

		if (!discarded)
			i += sprintf(str + i, " none");
	}
	else if (move->type == STEAL) {
		int r;
		for (r = 0; r < DESERT; r++)
			if (move->in[r])
				break;
		i += sprintf(str + i, "steal %s", r == DESERT ? "none" : resource_to_string(r));
	}
	else if (move->type == ROBBER) {
		i += sprintf(str + i, "robber %d", move->tile->index);
	}
	else if (move->type == DRAW) {
		i += sprintf(str + i, "draw %s", development_to_string(move->development));
	}
	else if (move->type == PLAYKNIGHT) {
		i += sprintf(str + i, "play knight");
	}
	else if (move->type == PLAYROADBUILDING) {
		i += sprintf(str + i, "play road building %d %d", move->rtile->index, move->rtile1->index);
	}
	else if (move->type == PLAYYEAROFPLENTY) {
		i += sprintf(str + i, "play year of plenty");
		for (int r = 0; r < DESERT; r++)
			if (move->in[r])
				i += sprintf(str + i, " %d %s", move->in[r], resource_to_string(r));
	}
	else if (move->type == PLAYMONOPOLY) {
		i += sprintf(str + i, "play monopoly %s", resource_to_string(move->number));
	}
}

void print_move(struct position *pos, struct move *move) {
	(void)pos;
	char str[256];
	move_to_str(move, str);
	printf("move %s", str);
}
