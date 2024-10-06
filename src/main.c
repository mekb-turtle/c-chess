#include <stdio.h>
#include <stdlib.h>

#include "chess.h"

int main() {
	struct game game;
	game.malloc = malloc;
	game.free = free;
	chess_init(&game);
	chess_print(&game);
	struct move_list *list = get_legal_moves(&game, (struct position){4, 1}); // e2
	for (struct move_list *move = list; move; move = move->next) {
		printf("Move: %c%c -> %c%c\n", file_to_char(move->move.from.x), rank_to_char(move->move.from.y), file_to_char(move->move.to.x), rank_to_char(move->move.to.y));
	}
}
