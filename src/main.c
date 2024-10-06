#include <stdio.h>
#include <stdlib.h>

#include "chess.h"

int main() {
	struct game game;
	game.malloc = malloc;
	game.free = free;
	chess_init(&game);
	chess_print(&game);
}
