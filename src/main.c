#include <stdio.h>
#include <stdlib.h>

#include "chess.h"

int main() {
	struct game *game = create_board(malloc, free);
	board_init(game);
	for (uint16_t i = 0; i < 500; ++i) {
		printf("%s's move\n", game->active_color == COLOR_WHITE ? "White" : "Black");
		print_board(game, true, true, stdout);

		if (game->win != STATE_NONE) {
			printf("Game over\n");
			break;
		}

		struct move_list *list = get_legal_moves(game);
		if (!list) {
			printf("No legal moves\n");
			break;
		}

		// pick first legal move
		// TODO: allow user to verse another player locally, via socket, or against an installed engine
		printf("Playing %s\n", list->move.notation);
		perform_move(game, list->move);

		free_move_list(game, list);
	}
	print_board(game, true, true, stdout);
	destroy_board(game);
}
