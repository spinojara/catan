enum {
	BUILD = 0x1,
	TRADE = 0x2,
	ENDTURN = 0x4,
	RANDOM = 0x8,
	SETUP = 0x10,
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

	/* Trade. */
	int out[DESERT];
	int in[DESERT];

	/* Build. */
	int building;

	struct smtile *smtile;
	struct rtile *rtile;

	/* Random. */
	int number;
};

/* 2 <= n <= 12, distributed as 2, D6 */
void generate_diceroll() {
}

void generate_setup(struct position *pos, struct move **moves, size_t *n) {
	int flip = (pos->setup % 2) ? 1 : 0;

	for (int s = 0; s < MAX_SETTLEMENTS; s++) {
		if (pos->smtiles[s].blocked || pos->smtiles[s].settlement || pos->smtiles[s].city)
			continue;

		/* Nothing can have been built on this rtile. */
		for (int r = 0; pos->smtiles[s].rtiles[r]; r++) {
			*moves = realloc(*moves, ++*n * sizeof(**moves));
			(*moves)[*n - 1].type = SETUP;
			(*moves)[*n - 1].smtile = &pos->smtiles[s];
			(*moves)[*n - 1].rtile = pos->smtiles[s].rtiles[r];
			(*moves)[*n - 1].flip = flip;
		}
	}
}

void generate_trade(const struct position *pos, struct move **moves, size_t *n) {
	const struct player *player = &pos->player[pos->turn];
	int trades[DESERT];
	for (int resource = 0; resource < DESERT; resource++)
		trades[resource] = player->resources[resource] / 4;

	for (int brick = 0; brick <= trades[BRICK]; brick++) {
	for (int lumber = 0; lumber <= trades[LUMBER]; lumber++) {
	for (int ore = 0; ore <= trades[ORE]; ore++) {
	for (int grain = 0; grain <= trades[GRAIN]; grain++) {
	for (int wool = 0; wool <= trades[WOOL]; wool++) {
		int total = brick + lumber + ore + grain + wool;
		if (!total)
			continue;
		for (int ib = 0; ib <= total; ib++) {
		for (int il = 0; il <= total - ib; il++) {
		for (int io = 0; io <= total - ib - il; io++) {
		for (int ig = 0; ig <= total - ib - il - io; ig++) {
			int iw = total - ib - il - io - ig;

			*moves = realloc(*moves, ++*n * sizeof(**moves));
			struct move *move = &(*moves)[*n - 1];

			move->out[BRICK] = 4 * brick;
			move->out[LUMBER] = 4 * lumber;
			move->out[ORE] = 4 * ore;
			move->out[GRAIN] = 4 * grain;
			move->out[WOOL] = 4 * wool;

			move->in[BRICK] = ib;
			move->in[LUMBER] = il;
			move->in[ORE] = io;
			move->in[GRAIN] = ig;
			move->in[WOOL] = iw;

			move->type = TRADE;
			move->flip = 0;
		}
		}
		}
		}
	}
	}
	}
	}
	}
}

void generate_build(struct position *pos, struct move **moves, size_t *n) {
	const struct player *player = &pos->player[pos->turn];

	if (player->resources[ORE] >= 3 && player->resources[GRAIN] >= 2 && player->city < 4) {
		for (struct smtile *smtile = pos->smtiles; smtile - pos->smtiles < MAX_SETTLEMENTS; smtile++) {
			if (smtile->player == pos->turn && smtile->settlement) {
				*moves = realloc(*moves, ++*n * sizeof(**moves));
				struct move *move = &(*moves)[*n - 1];
				move->building = CITY;
				move->smtile = smtile;
				move->type = BUILD;
				move->flip = 0;
			}
		}
	}

	if (player->resources[WOOL] >= 1 && player->resources[GRAIN] >= 1 && player->resources[LUMBER] >= 1 && player->resources[BRICK] >= 1 && player->settlement < 5) {
		for (struct smtile *smtile = pos->smtiles; smtile - pos->smtiles < MAX_SETTLEMENTS; smtile++) {
			if (!smtile->city && !smtile->settlement && !smtile->blocked && smtile->reachable[pos->turn]) {
				*moves = realloc(*moves, ++*n * sizeof(**moves));
				struct move *move = &(*moves)[*n - 1];
				move->building = SETTLEMENT;
				move->smtile = smtile;
				move->type = BUILD;
				move->flip = 0;
			}
		}
	}

	if (player->resources[LUMBER] >= 1 && player->resources[BRICK] >= 1 && player->road < 15) {
		for (struct rtile *rtile = pos->rtiles; rtile - pos->rtiles < MAX_ROADS; rtile++) {
			if (!rtile->road && rtile->reachable[pos->turn]) {
				*moves = realloc(*moves, ++*n * sizeof(**moves));
				struct move *move = &(*moves)[*n - 1];
				move->building = ROAD;
				move->rtile = rtile;
				move->type = BUILD;
				move->flip = 0;
			}
		}
	}

	if (player->resources[ORE] >= 1 && player->resources[GRAIN] >= 1 && player->resources[WOOL] >= 1) {
		*moves = realloc(*moves, ++*n * sizeof(**moves));
		struct move *move = &(*moves)[*n - 1];
		move->building = DEVELOPMENT;
		move->type = BUILD;
		move->flip = 0;
	}
}

void do_build(struct position *pos, struct move *move) {
	if (move->building == DEVELOPMENT) {
		// TODO

		pos->player[pos->turn].resources[ORE]--;
		pos->player[pos->turn].resources[GRAIN]--;
		pos->player[pos->turn].resources[WOOL]--;
	}
	else if (move->building == ROAD) {
		move->rtile->road = 1;
		move->rtile->player = pos->turn;

		for (int i = 0; move->rtile->rtiles[i]; i++)
			move->rtile->rtiles[i]->reachable[pos->turn]++;
		for (int i = 0; move->rtile->smtiles[i]; i++)
			move->rtile->smtiles[i]->reachable[pos->turn]++;

		pos->player[pos->turn].resources[BRICK]--;
		pos->player[pos->turn].resources[LUMBER]--;

		pos->player[pos->turn].road++;
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
		}

		pos->player[pos->turn].settlement += (move->building == SETTLEMENT) - (move->building == CITY);
		pos->player[pos->turn].city += move->building == CITY;
	}
}


void undo_build(struct position *pos, struct move *move) {
	if (move->building == DEVELOPMENT) {
		// TODO

		pos->player[pos->turn].resources[ORE]++;
		pos->player[pos->turn].resources[GRAIN]++;
		pos->player[pos->turn].resources[WOOL]++;
	}
	else if (move->building == ROAD) {
		move->rtile->road = 0;

		for (int i = 0; move->rtile->rtiles[i]; i++)
			move->rtile->rtiles[i]->reachable[pos->turn]--;
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
		}

		pos->player[pos->turn].settlement -= (move->building == SETTLEMENT) - (move->building == CITY);
		pos->player[pos->turn].city -= move->building == CITY;
	}
}

void do_random(struct position *pos, int n) {
	for (int i = 0; i < MAX_SETTLEMENTS; i++) {
		if (pos->smtiles[i].city || pos->smtiles[i].settlement) {
			for (int j = 0; pos->smtiles[i].tiles[j]; j++) {
				if (pos->smtiles[i].tiles[j]->number == n && pos->smtiles[i].tiles[j]->type != DESERT && !pos->smtiles[i].tiles[j]->robber) {
					pos->player[pos->smtiles[i].player].resources[pos->smtiles[i].tiles[j]->type] += 1 + pos->smtiles[i].city;
				}
			}
		}
	}
}

void undo_random(struct position *pos, int n) {
	for (int i = 0; i < MAX_SETTLEMENTS; i++) {
		if (pos->smtiles[i].city || pos->smtiles[i].settlement) {
			for (int j = 0; pos->smtiles[i].tiles[j]; j++) {
				if (pos->smtiles[i].tiles[j]->number == n && pos->smtiles[i].tiles[j]->type != DESERT && !pos->smtiles[i].tiles[j]->robber) {
					pos->player[pos->smtiles[i].player].resources[pos->smtiles[i].tiles[j]->type] -= 1 + pos->smtiles[i].city;
				}
			}
		}
	}
}

void do_trade(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] += move->in[r] - move->out[r];
}

void undo_trade(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] -= move->in[r] - move->out[r];
}

void generate(struct position *pos, struct move **moves, size_t *n) {
	if (pos->setup) {
		generate_setup(pos, moves, n);
	}
	else {
		*moves = realloc(*moves, ++*n * sizeof(**moves));
		(*moves)[*n - 1].type = ENDTURN;
		(*moves)[*n - 1].flip = 1;
		generate_build(pos, moves, n);
		generate_trade(pos, moves, n);
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

	pos->setup = (pos->setup + 4) % 5;

	if (pos->setup >= 3) {
		for (int t = 0; move->smtile->tiles[t]; t++)
			if (move->smtile->tiles[t]->type != DESERT)
				pos->player[pos->turn].resources[move->smtile->tiles[t]->type]--;
	}
}

void do_move(struct position *pos, struct move *move) {
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
	default:
		break;
	}

	if (move->flip)
		pos->turn = 1 - pos->turn;
}

void undo_move(struct position *pos, struct move *move) {
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
	default:
		break;
	}
}

void print_move(struct position *pos, struct move *move) {
	printf("move ");

	if (move->type == BUILD) {
		printf("build ");

		if (move->building == ROAD) {
			printf("road %ld", move->rtile - pos->rtiles);
		}
		else if (move->building == SETTLEMENT) {
			printf("settlement %ld", move->smtile - pos->smtiles);
		}
		else if (move->building == CITY) {
			printf("city %ld", move->smtile - pos->smtiles);
		}
		else {
			printf("development card");
		}
	}
	else if (move->type == SETUP) {
		printf("setup settlement %ld road %ld", move->smtile - pos->smtiles, move->rtile - pos->rtiles);
	}
	else if (move->type == TRADE) {
		printf("trade ");

		for (int r = 0; r < DESERT; r++) {
			if (move->out[r])
				printf("%d %s ", move->out[r], resource_to_string(r));
		}
		printf("->");
		for (int r = 0; r < DESERT; r++) {
			if (move->in[r])
				printf("%d %s ", move->in[r], resource_to_string(r));
		}
	}
	else if (move->type == ENDTURN) {
		printf("end turn");
	}
	else if (move->type == RANDOM) {
		printf("random");
	}
}
