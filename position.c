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
	int player;
	int city;
	int settlement;

	/* Blocked by nerby settlement. */
	int blocked;

	struct tile *tiles[3];
};

/* road tile */
struct rtile {
	int player;
	int road;

	struct smtile *smtiles[2];
	struct rtile *rtiles[4];
};

struct tile {
	int type;
	int number;
	int robber;
};

struct player {
	int resources[DESERT];
};

struct position {
	int turn;

	struct player player[2];

	struct tile tiles[MAX_TILES];
	struct smtile smtiles[MAX_SETTLEMENTS];
	struct rtile rtiles[MAX_ROADS];
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

void position_init(struct position *pos) {
	memset(pos, 0, sizeof(*pos));

	pos->smtiles[0].tiles[0] = &pos->tiles[0];
	pos->smtiles[1].tiles[0] = &pos->tiles[1];
	pos->smtiles[2].tiles[0] = &pos->tiles[2];
	pos->smtiles[3].tiles[0] = &pos->tiles[0];
	pos->smtiles[4].tiles[0] = &pos->tiles[0];
	pos->smtiles[4].tiles[1] = &pos->tiles[1];
	pos->smtiles[5].tiles[0] = &pos->tiles[1];
	pos->smtiles[5].tiles[1] = &pos->tiles[2];



	pos->rtiles[0].rtiles[0] = &pos->rtiles[1];
	pos->rtiles[0].rtiles[1] = &pos->rtiles[6];
	pos->rtiles[1].rtiles[0] = &pos->rtiles[0];
	pos->rtiles[1].rtiles[1] = &pos->rtiles[2];
	pos->rtiles[1].rtiles[2] = &pos->rtiles[7];



	pos->rtiles[0].smtiles[0] = &pos->smtiles[0];
	pos->rtiles[0].smtiles[1] = &pos->smtiles[3];
	pos->rtiles[1].smtiles[0] = &pos->smtiles[0];
	pos->rtiles[1].smtiles[1] = &pos->smtiles[4];
}

void randomize_position(struct position *pos) {
	int numbers[18] = { 2, 3, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 10, 11, 11, 12 };
	int resources[18] = { BRICK, BRICK, BRICK, LUMBER, LUMBER, LUMBER, LUMBER, ORE, ORE, ORE, GRAIN, GRAIN, GRAIN, GRAIN, WOOL, WOOL, WOOL, WOOL };

	/* Fisher-Yates */
	for (int i = 17; i >= 1; i--) {
		int j = rand() % (i + 1);

		int a = resources[i];
		resources[i] = resources[j];
		resources[j] = a;
	}

	for (int i = 17; i >= 1; i--) {
		int j = rand() % (i + 1);

		int a = numbers[i];
		numbers[i] = numbers[j];
		numbers[j] = a;
	}


	int desert = rand() % MAX_SETTLEMENTS;

	for (int i = 0; i < MAX_SETTLEMENTS; i++) {
		if (i == desert) {
			pos->tiles[i].number = 0;
			pos->tiles[i].type = DESERT;
			pos->tiles[i].robber = 1;
		}
		else {
			int realindex = i - (i > desert);
			pos->tiles[i].number = numbers[realindex];
			pos->tiles[i].type = resources[realindex];
			pos->tiles[i].robber = 0;
		}
	}
}
