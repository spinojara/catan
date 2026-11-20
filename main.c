#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "position.c"
#include "move.c"

int main(void) {
	srand(time(NULL));

	struct position pos = { 0 };

	randomize_position(&pos);

	position_print(&pos);
}
