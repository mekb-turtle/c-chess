#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include "input.h"
#include "chess.h"
#include "display.h"

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

struct options {
	struct player {
		enum player_type {
			PLAYER_LOCAL,
			PLAYER_ENGINE,
			PLAYER_SOCKET,
		} type;
		char *path;
	} player1, player2;
	enum piece_color player1_color;
	struct display_settings display;
	char *socket;
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
        .display = {
                    .unicode = false,
                    .color = true}
};

static enum player_type get_player_type(enum piece_color color) {
	if (options.player1_color == color) return options.player1.type;
	return options.player2.type;
}

void print_board_opt(struct game *game) {
	print_board(options.display, game, stdout);
}

static struct game *game = NULL;
static bool clean_exit = false;

void exit_func(int sig) {
	if (sig != 0) eprintf("\nCaught signal %d\n", sig);
	input_exit(stdin);
	destroy_board(game);
	game = NULL;
	if (sig == 0) {
		eprintf("Exiting\n");
		return; // atexit cannot call exit
	}
	if (clean_exit) exit(0);    // exit normally
	if (sig == SIGINT) exit(0); // exit normally
	exit(sig + 128);
}

void atexit_func(void) {
	exit_func(0);
}

static void parse_player(char *str, struct player *player, bool *invalid) {
	if (*invalid) return;
	if (!str) goto invalid;
	if (strcmp(str, "player") == 0 || strcmp(str, "p") == 0) {
		player->type = PLAYER_LOCAL;
		player->path = NULL;
	} else if (strncmp(str, "engine:", 7) == 0 || strncmp(str, "e:", 2) == 0) {
		char *p = strchr(str, ':');
		if (!p || p[1] == '\0') goto invalid;
		player->type = PLAYER_ENGINE;
		player->path = &p[1];
	} else if (strncmp(str, "socket:", 7) == 0 || strncmp(str, "s:", 2) == 0) {
		char *p = strchr(str, ':');
		if (!p || p[1] == '\0') goto invalid;
		player->type = PLAYER_SOCKET;
		player->path = &p[1];
	} else {
	invalid:
		*invalid = true;
	}
}

static void parse_bool(char *str, bool *value, bool *invalid) {
	if (*invalid) return;
	if (!str) {
		*value = true;
		return;
	}
	if (strcmp(str, "yes") == 0 || strcmp(str, "y") == 0 || strcmp(str, "on") == 0)
		*value = true;
	else if (strcmp(str, "no") == 0 || strcmp(str, "n") == 0 || strcmp(str, "off") == 0)
		*value = false;
	else
		*invalid = true;
}

int main(int argc, char *argv[]) {
	srand(time(NULL));

	bool invalid = false, player1_set = false, player2_set = false, player1_color_set = false, unicode_set = false, color_set = false, space_set = false;

	int opt;
	while ((opt = getopt_long(argc, argv, ":hV1:2:c:u:C:T:s:", (struct option[]){
	                                                                   {"help",          no_argument,       0, 'h'},
	                                                                   {"version",       no_argument,       0, 'V'},
	                                                                   {"player1",       required_argument, 0, '1'},
	                                                                   {"player2",       required_argument, 0, '2'},
	                                                                   {"player1_color", required_argument, 0, 'c'},
	                                                                   {"unicode",       required_argument, 0, 'u'},
	                                                                   {"color",         required_argument, 0, 'C'},
	                                                                   {"space",         required_argument, 0, 'T'},
	                                                                   {"socket",        required_argument, 0, 's'},
	                                                                   {0,               0,                 0, 0  }
    },
	                          NULL)) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s [options)\n", argv[0]);
				printf("Options:\n");
				printf("  -h, --help\n");
				printf("  -V, --version\n");
				printf("  -1, --player1 (player|socket:<path>|engine:<path>)\n");
				printf("  -2, --player2 (player|socket:<path>|engine:<path>)\n");
				printf("  -c, --player1_color (white|black|random)\n");
				printf("  -u, --unicode (on|yes|off|no)\n");
				printf("  -C, --color (on|yes|off|no)\n");
				printf("  -T, --space (on|yes|off|no)\n");
				printf("  -S, --socket <path> - Connect to player socket (incompatible with -1, -2, -q)\n");
				return 0;
			case 'V':
				printf("Chess %s\n", PROJECT_VERSION);
				return 0;
			case '1':
				if (player1_set) invalid = true;
				else
					parse_player(optarg, &options.player1, &invalid);
				player1_set = true;
				break;
			case '2':
				if (player2_set) invalid = true;
				else
					parse_player(optarg, &options.player2, &invalid);
				player2_set = true;
				break;
			case 'c':
				if (player1_color_set) invalid = true;
				else if (!optarg)
					invalid = true;
				else if (strcmp(optarg, "white") == 0 || strcmp(optarg, "w") == 0)
					options.player1_color = COLOR_WHITE;
				else if (strcmp(optarg, "black") == 0 || strcmp(optarg, "b") == 0)
					options.player1_color = COLOR_BLACK;
				else if (strcmp(optarg, "random") == 0 || strcmp(optarg, "r") == 0)
					options.player1_color = rand() % 2 ? COLOR_WHITE : COLOR_BLACK;
				else
					invalid = true;
				player1_color_set = true;
				break;
			case 'u':
				if (unicode_set) invalid = true;
				else
					parse_bool(optarg, &options.display.unicode, &invalid);
				unicode_set = true;
				break;
			case 'C':
				if (color_set) invalid = true;
				else
					parse_bool(optarg, &options.display.color, &invalid);
				color_set = true;
				break;
			case 'T':
				if (space_set) invalid = true;
				else
					parse_bool(optarg, &options.display.extra_space, &invalid);
				space_set = true;
				break;
			case 'S':
				if (options.socket) invalid = true;
				else
					options.socket = optarg;
				break;
			default:
				invalid = true;
				break;
		}
	}

	if (optind != argc || invalid) {
		eprintf("Invalid arguments\nTry --help for help\n");
		exit(1);
	}

	options.display.view_flip = options.player1_color == COLOR_BLACK;

	// handle signals
	int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT, 0};
	for (int i = 0; signals[i]; i++)
		signal(signals[i], exit_func);
	atexit(atexit_func);

	game = create_board(malloc, free);
	board_init(game);
	while (true) {
		print_board_opt(game);

		struct move_list *list = get_legal_moves(game);
		if (!list) {
			eprintf("No legal moves\n");
			break;
		}

		if (game->win != STATE_NONE) {
			printf("Game over\n");
			break;
		}

		switch (get_player_type(game->active_color)) {
			case PLAYER_LOCAL:;
				free_move_list(game, list);
				struct move move = prompt_for_move(options.display, game, stdout, stdin, &options.display.view_flip, print_board_opt);
				if (!perform_move(game, move)) {
					eprintf("Failed to perform move\n");
					exit(1);
				}
				break;
			case PLAYER_ENGINE:
			case PLAYER_SOCKET:;
				printf("%s's move\n", game->active_color == COLOR_WHITE ? "White" : "Black");
				// TODO: implement
				// pick first legal move
				printf("Playing %s\n", list->move.notation);
				if (!perform_move(game, list->move)) {
					eprintf("Failed to perform move\n");
					exit(1);
				}
				free_move_list(game, list);
				break;
		}
	}
	clean_exit = true;
	atexit_func();
}
