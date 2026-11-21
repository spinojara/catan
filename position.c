#include <string.h>

#define IMAGE_X 111
#define IMAGE_Y 54

#define MAX_TILES 19
#define MAX_ROADS 72
#define MAX_SETTLEMENTS 54


enum {
	BRICK,
	LUMBER,
	ORE,
	GRAIN,
	WOOL,
	DESERT
};

/* settlement tile */
struct smtile {
	int index;

	int player;
	int city;
	int settlement;

	/* Blocked by nerby settlement. */
	int blocked;

	int reachable[2];

	/* One more so that we end with NULL. */
	struct tile *tiles[4];
	struct rtile *rtiles[4];
};

/* road tile */
struct rtile {
	int index;

	int player;
	int road;

	int reachable[2];

	struct smtile *smtiles[3];
	struct rtile *rtiles[5];
};

struct tile {
	int index;

	int type;
	int number;
	int robber;
};

struct player {
	int resources[DESERT];

	int road;
	int settlement;
	int city;

	int score;

	int longestroad;
};

struct position {
	int setup;

	int seven;
	int dice;

	int robber;

	int turn;

	struct player player[2];

	struct tile tiles[MAX_TILES];
	struct smtile smtiles[MAX_SETTLEMENTS];
	struct rtile rtiles[MAX_ROADS];

	struct smtile *common_smtile[MAX_ROADS][MAX_ROADS];
};

char *resource_to_string(int resource) {
	static char str[7];

	switch (resource) {
	case BRICK:
		sprintf(str, "brick");
		break;
	case LUMBER:
		sprintf(str, "lumber");
		break;
	case ORE:
		sprintf(str, "ore");
		break;
	case GRAIN:
		sprintf(str, "grain");
		break;
	case WOOL:
		sprintf(str, "wool");
		break;
	case DESERT:
		sprintf(str, "desert");
		break;
	}

	return str;
}

void print_tile(char image[IMAGE_Y][IMAGE_X], struct tile *tile, int x, int y) {
	for (int i = -2; i <= 3; i++) {
		image[y + i][x - 11] = '|';
		image[y + i][x + 11] = '|';
	}

	char *str = resource_to_string(tile->type);

	size_t n = strlen(str);

	sprintf(&image[y][x - (n - 1) / 2], "%s", str);
	sprintf(&image[y + 1][x], "%d", tile->number);

	if (tile->robber)
		sprintf(&image[y + 2][x - 2], "robber");

	for (int i = -1; i <= 2; i++) {
		image[y + 5 + i][x + 7 - 3 * i] = '/';
		image[y - 4 - i][x + 7 - 3 * i] = '\\';
	}

	for (int i = -2; i <= 1; i++) {
		image[y - 4 + i][x - 7 - 3 * i] = '/';
		image[y + 5 - i][x - 7 - 3 * i] = '\\';
	}
}

void position_print(struct position *pos) {
	char image[IMAGE_Y][IMAGE_X] = { 0 };


	print_tile(image, &pos->tiles[0], 33, 6);
	print_tile(image, &pos->tiles[1], 55, 6);
	print_tile(image, &pos->tiles[2], 77, 6);

	print_tile(image, &pos->tiles[3], 22, 16);
	print_tile(image, &pos->tiles[4], 44, 16);
	print_tile(image, &pos->tiles[5], 66, 16);
	print_tile(image, &pos->tiles[6], 88, 16);

	print_tile(image, &pos->tiles[7], 11, 26);
	print_tile(image, &pos->tiles[8], 33, 26);
	print_tile(image, &pos->tiles[9], 55, 26);
	print_tile(image, &pos->tiles[10], 77, 26);
	print_tile(image, &pos->tiles[11], 99, 26);

	print_tile(image, &pos->tiles[12], 22, 36);
	print_tile(image, &pos->tiles[13], 44, 36);
	print_tile(image, &pos->tiles[14], 66, 36);
	print_tile(image, &pos->tiles[15], 88, 36);

	print_tile(image, &pos->tiles[16], 33, 46);
	print_tile(image, &pos->tiles[17], 55, 46);
	print_tile(image, &pos->tiles[18], 77, 46);

	for (int y = 0; y < IMAGE_Y; y++) {
		for (int x = 0; x < IMAGE_X; x++) {
			printf("%c", image[y][x] ? image[y][x] : ' ');
		}
		printf("\n");
	}
}

static const int road_settlements[MAX_ROADS][2] = {
	/* 0 - 5 */
	{ 0, 3 }, { 0, 4 }, { 1, 4 }, { 1, 5 }, { 2, 5 }, { 2, 6 },
	/* 6 - 9 */
	{ 3, 7 }, { 4, 8 }, { 5, 9 }, { 6, 10 },
	/* 10 - 17 */
	{ 7, 11 }, { 7, 12 }, { 8, 12 }, { 8, 13 }, { 9, 13 }, { 9, 14 }, { 10, 14 }, { 10, 15 },
	/* 18 - 22 */
	{ 11, 16 }, { 12, 17 }, { 13, 18 }, { 14, 19 }, { 15, 20 },
	/* 23 - 32 */
	{ 16, 21 }, { 16, 22 }, { 17, 22 }, { 17, 23 }, { 18, 23 }, { 18, 24 }, { 19, 24 }, { 19, 25 }, { 20, 25 }, { 20, 26 },
	/* 33 - 38 */
	{ 21, 27 }, { 22, 28 }, { 23, 29 }, { 24, 30 }, { 25, 31 }, { 26, 32 },
	/* 39 - 48 */
	{ 27, 33 }, { 28, 33 }, { 28, 34 }, { 29, 34 }, { 29, 35 }, { 30, 35 }, { 30, 36 }, { 31, 36 }, { 31, 37 }, { 32, 37 },
	/* 49 - 53 */
	{ 33, 38 }, { 34, 39 }, { 35, 40 }, { 36, 41 }, { 37, 42 },
	/* 54 - 61 */
	{ 38, 43 }, { 39, 43 }, { 39, 44 }, { 40, 44 }, { 40, 45 }, { 41, 45 }, { 41, 46 }, { 42, 46 },
	/* 62 - 65 */
	{ 43, 47 }, { 44, 48 }, { 45, 49 }, { 46, 50 },
	/* 66 - 71 */
	{ 47, 51 }, { 48, 51 }, { 48, 52 }, { 49, 52 }, { 49, 53 }, { 50, 53 },
};

static const int tile_settlements[MAX_TILES][6] = {
	{ 0, 3, 4, 7, 8, 12 },
	{ 1, 4, 5, 8, 9, 13 },
	{ 2, 5, 6, 9, 10, 14 },

	{ 7, 11, 12, 16, 17, 22 },
	{ 8, 12, 13, 17, 18, 23 },
	{ 9, 13, 14, 18, 19, 24 },
	{ 10, 14, 15, 19, 20, 25 },

	{ 16, 21, 22, 27, 28, 33 },
	{ 17, 22, 23, 28, 29, 34 },
	{ 18, 23, 24, 29, 30, 35 },
	{ 19, 24, 25, 30, 31, 36 },
	{ 20, 25, 26, 31, 32, 37 },

	{ 28, 33, 34, 38, 39, 43 },
	{ 29, 34, 35, 39, 40, 44 },
	{ 30, 35, 36, 40, 41, 45 },
	{ 31, 36, 37, 41, 42, 46 },

	{ 39, 43, 44, 47, 48, 51 },
	{ 40, 44, 45, 48, 49, 52 },
	{ 41, 45, 46, 49, 50, 53 },
};

void position_init(struct position *pos) {
	memset(pos, 0, sizeof(*pos));

	for (int t = 0; t < MAX_TILES; t++) {
		pos->tiles[t].index = t;

		for (int i = 0; i < 6; i++) {
			struct smtile *smtile = &pos->smtiles[tile_settlements[t][i]];
			int n = 0;
			while (smtile->tiles[n])
				n++;
			smtile->tiles[n] = &pos->tiles[t];
		}
	}

	/* Give each road two settlements, and every other road that
	 * shares one of those settlements.
	 */
	for (int r = 0; r < MAX_ROADS; r++) {
		struct rtile *rtile = &pos->rtiles[r];
		rtile->index = r;
		const int *ends = road_settlements[r];

		rtile->smtiles[0] = &pos->smtiles[ends[0]];
		rtile->smtiles[1] = &pos->smtiles[ends[1]];

		int n = 0;
		for (int s = 0; s < MAX_ROADS; s++) {
			if (s == r)
				continue;
			const int *other = road_settlements[s];
			if (other[0] == ends[0] || other[0] == ends[1] || other[1] == ends[0] || other[1] == ends[1])
				rtile->rtiles[n++] = &pos->rtiles[s];
		}
	}

	for (int s = 0; s < MAX_SETTLEMENTS; s++) {
		int n = 0;
		struct smtile *smtile = &pos->smtiles[s];
		smtile->index = s;
		for (int r = 0; r < MAX_ROADS; r++) {
			struct rtile *rtile = &pos->rtiles[r];

			for (int i = 0; rtile->smtiles[i]; i++) {
				if (rtile->smtiles[i] == smtile)
					smtile->rtiles[n++] = rtile;
			}
		}
	}

	pos->setup = 1;

	for (int r1 = 0; r1 < MAX_ROADS; r1++) {
		for (int r2 = 0; r2 < MAX_ROADS; r2++) {
			if (r1 == r2)
				continue;

			for (int s1 = 0; pos->rtiles[r1].smtiles[s1]; s1++) {
				for (int s2 = 0; pos->rtiles[r2].smtiles[s2]; s2++) {
					if (pos->rtiles[r1].smtiles[s1] == pos->rtiles[r2].smtiles[s2])
						pos->common_smtile[r1][r2] = pos->rtiles[r1].smtiles[s1];
				}
			}
		}
	}
}

void randomize_position(struct position *pos, uint64_t *seed) {
	int numbers[18] = { 2, 3, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 10, 11, 11, 12 };
	int resources[18] = { BRICK, BRICK, BRICK, LUMBER, LUMBER, LUMBER, LUMBER, ORE, ORE, ORE, GRAIN, GRAIN, GRAIN, GRAIN, WOOL, WOOL, WOOL, WOOL };

	/* Fisher-Yates */
	for (int i = 17; i >= 1; i--) {
		int j = uniformint(seed, 0, i + 1);

		int a = resources[i];
		resources[i] = resources[j];
		resources[j] = a;
	}

	for (int i = 17; i >= 1; i--) {
		int j = uniformint(seed, 0, i + 1);

		int a = numbers[i];
		numbers[i] = numbers[j];
		numbers[j] = a;
	}

	int desert = uniformint(seed, 0, MAX_TILES);

	for (int i = 0; i < MAX_TILES; i++) {
		if (i == desert) {
			pos->tiles[i].number = 0;
			pos->tiles[i].type = DESERT;
			pos->tiles[i].robber = 1;
			pos->robber = i;
		}
		else {
			int realindex = i - (i > desert);
			pos->tiles[i].number = numbers[realindex];
			pos->tiles[i].type = resources[realindex];
			pos->tiles[i].robber = 0;
		}
	}

	pos->turn = 1;
}

int gameover(struct position *pos) {
	if (pos->player[pos->turn].score >= 8) {
		return 1;
	}
	else if (pos->player[1 - pos->turn].score >= 8) {
		return -1;
	}
	return 0;
}
