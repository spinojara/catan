enum {
	BUILD = 0x1,
	TRADE = 0x2,
	ENDTURN = 0x4,
	RANDOM = 0x8,
	SETUP = 0x10,
	DISCARD = 0x20,
	STEAL = 0x40,
};

enum {
	CHOICE,
	UNIFORM,
	DICE,
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

	/* Trade or discard cards. */
	int out[DESERT];
	int in[DESERT];

	/* Build. */
	int building;

	/* If building == SETTLEMENT then this might make a road unreachable. */
	int unreachable[3];

	struct smtile *smtile;
	struct rtile *rtile;

	/* Random. */
	int number;

	/* Old longest road. */
	int prevroad[2];
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
			/* No opponent road can touch a spot that is free in the setup. */
			memset((*moves)[*n - 1].unreachable, 0, sizeof((*moves)[*n - 1].unreachable));
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
		for (int s = 0; s < MAX_SETTLEMENTS; s++) {
			struct smtile *smtile = &pos->smtiles[s];
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
		for (int s = 0; s < MAX_SETTLEMENTS; s++) {
			struct smtile *smtile = &pos->smtiles[s];
			if (!smtile->city && !smtile->settlement && !smtile->blocked && smtile->reachable[pos->turn]) {
				*moves = realloc(*moves, ++*n * sizeof(**moves));
				struct move *move = &(*moves)[*n - 1];
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
		}

		pos->player[pos->turn].settlement -= (move->building == SETTLEMENT) - (move->building == CITY);
		pos->player[pos->turn].city -= move->building == CITY;
	}
}

void do_random(struct position *pos, int n) {
	/* If we are already at a seven when a random move occurs, this is actually picking one of the other players cards. */
	if (pos->seven) {
	}
	else {
		if (n == 7) {
			pos->seven = 0;
		}
		for (int i = 0; i < MAX_SETTLEMENTS; i++) {
			if (pos->smtiles[i].city || pos->smtiles[i].settlement) {
				for (int j = 0; pos->smtiles[i].tiles[j]; j++) {
					if (pos->smtiles[i].tiles[j]->number == n && pos->smtiles[i].tiles[j]->type != DESERT && !pos->smtiles[i].tiles[j]->robber) {
						pos->player[pos->smtiles[i].player].resources[pos->smtiles[i].tiles[j]->type] += 1 + pos->smtiles[i].city;
					}
				}
			}
		}

		pos->dice = 0;
	}
}

void undo_random(struct position *pos, int n) {
	/* If we are already at a seven when a random move occurs, this is actually picking one of the other players cards. */
	if (pos->seven) {
	}
	else {
		if (n == 7) {
			pos->seven = 0;
		}
		for (int i = 0; i < MAX_SETTLEMENTS; i++) {
			if (pos->smtiles[i].city || pos->smtiles[i].settlement) {
				for (int j = 0; pos->smtiles[i].tiles[j]; j++) {
					if (pos->smtiles[i].tiles[j]->number == n && pos->smtiles[i].tiles[j]->type != DESERT && !pos->smtiles[i].tiles[j]->robber) {
						pos->player[pos->smtiles[i].player].resources[pos->smtiles[i].tiles[j]->type] -= 1 + pos->smtiles[i].city;
					}
				}
			}
		}

		pos->dice = 1;
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

void generate_seven(struct position *pos, struct move **moves, size_t *n) {
	struct player *player = &pos->player[pos->turn];
	int sum = player->resources[BRICK] + player->resources[LUMBER] + player->resources[GRAIN] + player->resources[WOOL] + player->resources[ORE];
	if (sum <= 7) {
		*moves = realloc(*moves, ++*n * sizeof(**moves));
		(*moves)[*n - 1].type = DISCARD;
		memset((*moves)[*n - 1].out, 0, sizeof((*moves)[*n - 1].out));
		(*moves)[*n - 1].flip = 1;
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

			*moves = realloc(*moves, ++*n * sizeof(**moves));
			(*moves)[*n - 1].type = DISCARD;
			(*moves)[*n - 1].out[BRICK] = tb;
			(*moves)[*n - 1].out[LUMBER] = tl;
			(*moves)[*n - 1].out[GRAIN] = tg;
			(*moves)[*n - 1].out[WOOL] = tw;
			(*moves)[*n - 1].out[ORE] = to;
			(*moves)[*n - 1].flip = 1;
		}
		}
		}
		}
	}
}

int generate(struct position *pos, struct move **moves, size_t *n) {
	if (pos->dice) {
		*n = 11;
		*moves = realloc(*moves, *n * sizeof(**moves));
		for (size_t i = 0; i < *n; i++) {
			(*moves)[i].type = RANDOM;
			(*moves)[i].number = i + 2;
			(*moves)[i].flip = 0;
		}
		return DICE;
	}
	else {
		if (pos->seven) {
			generate_seven(pos, moves, n);
		}
		else if (pos->setup) {
			generate_setup(pos, moves, n);
		}
		else {
			*moves = realloc(*moves, ++*n * sizeof(**moves));
			(*moves)[*n - 1].type = ENDTURN;
			(*moves)[*n - 1].flip = 1;
			generate_build(pos, moves, n);
			generate_trade(pos, moves, n);
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
}

void undo_discard(struct position *pos, struct move *move) {
	for (int r = 0; r < DESERT; r++)
		pos->player[pos->turn].resources[r] += move->out[r];
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
	case DISCARD:
		do_discard(pos, move);
		break;
	case RANDOM:
		do_random(pos, move->number);
		break;
	default:
		break;
	}

	if (move->flip)
		pos->turn = 1 - pos->turn;

	if (move->flip && !pos->setup)
		pos->dice = 1;
}

void undo_move(struct position *pos, struct move *move) {
	/* Take back the dice roll that do_move queued up. */
	if (move->flip && !pos->setup)
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
		i += sprintf(str + i, "random");
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
}

void print_move(struct position *pos, struct move *move) {
	(void)pos;
	char str[256];
	move_to_str(move, str);
	printf("move %s", str);
}
