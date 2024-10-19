#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include "input.h"
#include "chess.h"
#include "display.h"

struct options {
	struct player {
		enum player_type {
			PLAYER_LOCAL,
			PLAYER_ENGINE,
			PLAYER_SOCKET,
		} type;
		char *path;
	} player1, player2;
	enum player_color {
		PLAYER_WHITE = 0,
		PLAYER_BLACK = 1,
		PLAYER_RANDOM = 2
	} player1_color_select;
	enum piece_color player1_color;
	bool view_flip;
	struct display_settings display;
};

char *player_type_to_str(enum player_type type) {
	switch (type) {
		case PLAYER_LOCAL:
			return "Local";
		case PLAYER_ENGINE:
			return "Engine";
		case PLAYER_SOCKET:
			return "Socket";
	}
	return "Unknown";
}

static struct options options = {
        .player1 = {.type = PLAYER_LOCAL},
        .player2 = {.type = PLAYER_LOCAL},
        .player1_color_select = PLAYER_WHITE,
        .display = {
                    .unicode = false,
                    .color = true}
};

static enum player_type get_player_type(enum piece_color color) {
	if (options.player1_color == color) return options.player1.type;
	return options.player2.type;
}

void print_board_opt(struct game *game) {
	print_board(options.display, options.view_flip, game, stdout);
}

static struct game *game = NULL;

void exit_func(int sig) {
	printf("Caught signal %d\n", sig);
	if (game)
		destroy_board(game);
	game = NULL;
	if (sig == 0 || sig == SIGINT) exit(0); // exit normally
	exit(sig + 128);
}

void atexit_func(void) {

}

int main() {
	// handle signals
	int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT, 0};
	for (int i = 0; signals[i]; i++)
		signal(signals[i], exit_func);
	atexit(atexit_func);

	setlocale(LC_ALL, "C");
	srand(time(NULL));

	while (true) {
		printf("Player 1 (Q/W): %s", player_type_to_str(options.player1.type));
		if (options.player1.type != PLAYER_LOCAL && options.player1.path)
			printf(" (%s)", options.player1.path);
		printf("%*s", 10, "");
		printf("\n");
		printf("Player 2 (A/S): %s", player_type_to_str(options.player2.type));
		if (options.player2.type != PLAYER_LOCAL && options.player2.path)
			printf(" (%s)", options.player2.path);
		printf("%*s", 10, "");
		printf("\n");
		printf("Player 1 Color (E): ");
		switch (options.player1_color_select) {
			case PLAYER_WHITE:
			case PLAYER_BLACK:
				print_color(options.display, (enum piece_color) options.player1_color_select, stdout);
				break;
			case PLAYER_RANDOM:
				printf("Random");
				break;
		}
		printf("%*s", 10, "");
		printf("\n");
		printf("Unicode (U): ");
		print_bool(options.display, options.display.unicode, stdout);
		printf("%*s", 10, "");
		printf("\n");
		printf("Color (I): ");
		print_bool(options.display, options.display.color, stdout);
		printf("%*s", 10, "");
		printf("\n");
		printf("Start (Enter)\n");
		switch (tolower(scan_char(stdin, true))) {
			case 'q':
				options.player1.type = (options.player1.type + 1) % 3;
				break;
			case 'a':
				options.player2.type = (options.player2.type + 1) % 3;
				break;
			case 'e':
				options.player1_color_select = (options.player1_color_select + 1) % 3;
				break;
			case 'u':
				options.display.unicode = !options.display.unicode;
				break;
			case 'i':
				options.display.color = !options.display.color;
				break;
			case '\n':
			case '\r':
				goto start;
		}
		printf("\x1b[6A");
	}
	if (options.player1_color_select == PLAYER_RANDOM)
		options.player1_color = rand() % 2 ? COLOR_WHITE : COLOR_BLACK;
	else
		options.player1_color = (enum piece_color) options.player1_color_select;
	options.view_flip = options.player1_color == COLOR_BLACK;

start:;
	game = create_board(malloc, free);
	board_init(game);
	while (true) {
		print_board_opt(game);

		if (game->win != STATE_NONE) {
			printf("Game over\n");
			break;
		}

		switch (get_player_type(game->active_color)) {
			case PLAYER_LOCAL:;
				struct move move = prompt_for_move(options.display, game, stdout, stdin, &options.view_flip, print_board_opt);
				if (!perform_move(game, move)) {
					printf("Failed to perform move\n");
					exit(1);
				}
				break;
			case PLAYER_ENGINE:
			case PLAYER_SOCKET:;
				printf("%s's move\n", game->active_color == COLOR_WHITE ? "White" : "Black");
				// TODO: implement
				struct move_list *list = get_legal_moves(game);
				if (!list) {
					printf("No legal moves\n");
					break;
				}
				// pick first legal move
				printf("Playing %s\n", list->move.notation);
				if (!perform_move(game, list->move)) {
					printf("Failed to perform move\n");
					exit(1);
				}
				free_move_list(game, list);
				break;
		}
	}
	print_board_opt(game);
	exit_func(0);
}
