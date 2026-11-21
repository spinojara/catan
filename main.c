#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "position.c"
#include "move.c"
#include "search.c"

int main(void) {
	srand(time(NULL));

	struct position pos;

	position_init(&pos);
	randomize_position(&pos);

	struct node *root = calloc(1, sizeof(*root));
	uint64_t seed = 127387412;

	position_print(&pos);
	mcts(&pos, 30 * 1000, &root, &seed);
}
