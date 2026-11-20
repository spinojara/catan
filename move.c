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
struct trade {
	int out[DESERT];
	int in[DESERT];
};

struct build {
	int building;

	struct smtile *smtile;
	struct rtile *rtile;
};

struct endturn {
};

struct cutcards {
	int out[DESERT];
};

/* 2 <= n <= 12, distributed as 2, D6 */
struct randommove {
	int n;
};

void generate_trade(const struct position *pos) {
	struct trade trade;
	const struct player *player = &pos->player[pos->turn];
	int trades[DESERT];
	for (int resource = 0; resource < DESERT; resource++)
		trades[resource] = player->resources[resource] / 4;


	for (int brick = 0; brick <= trades[BRICK]; brick++) {
	for (int lumber = 0; lumber <= trades[LUMBER]; lumber++) {
	for (int ore = 0; ore <= trades[ORE]; ore++) {
	for (int grain = 0; grain <= trades[GRAIN]; grain++) {
	for (int wool = 0; wool <= trades[WOOL]; wool++) {
		int n = brick + lumber + ore + grain + wool;
		if (!n)
			continue;
		trade.out[BRICK] = 4 * brick;
		trade.out[LUMBER] = 4 * lumber;
		trade.out[ORE] = 4 * ore;
		trade.out[GRAIN] = 4 * grain;
		trade.out[WOOL] = 4 * wool;
		/* C(n + 4, 4) */
		int combinations = (n + 4) * (n + 3) * (n + 2) * (n + 1) / 24;
		for (int c = 0; c < combinations; c++) {
			/* which combination are we in? */
			trade.in[BRICK] = 0;
		}
	}
	}
	}
	}
	}
}

void generate_build(struct position *pos) {
	const struct player *player = &pos->player[pos->turn];
	struct build build;
	if (player->resources[ORE] >= 3 && player->resources[GRAIN] >= 2) {
		for (struct smtile *smtile = pos->smtiles; smtile - pos->smtiles < MAX_SETTLEMENTS; smtile++) {
			if (smtile->player == pos->turn && smtile->settlement) {
				build.building = CITY;
				build.smtile = smtile;
			}
		}
	}

	if (player->resources[WOOL] >= 1 && player->resources[GRAIN] >= 1 && player->resources[LUMBER] >= 1 && player->resources[BRICK] >= 1) {
		for (struct smtile *smtile = pos->smtiles; smtile - pos->smtiles < MAX_SETTLEMENTS; smtile++) {
			if (!smtile->city && !smtile->settlement && !smtile->blocked) {
				build.building = SETTLEMENT;
				build.smtile = smtile;
			}
		}
	}

	if (player->resources[LUMBER] >= 1 && player->resources[BRICK] >= 1) {
		for (struct rtile *rtile = pos->rtiles; rtile - pos->rtiles < MAX_ROADS; rtile++) {
			if (!rtile->road) {
				build.building = ROAD;
				build.rtile = rtile;
			}
		}
	}
}
