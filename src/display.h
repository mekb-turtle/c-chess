#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdbool.h>
#include <stdio.h>
#include "chess.h"

struct display_settings {
	bool unicode, color, view_flip, extra_space;
};
enum text_color {
	TEXT_WHITE = 0,
	TEXT_BLACK = 1,
	TEXT_NONE = 2,
	TEXT_RED = 3
};
void print_colored(struct display_settings display, enum text_color color, char *str, FILE *fp);
void print_moves(struct game *game, FILE *fp);
void print_board(struct display_settings display, struct game *game, FILE *fp);
#endif
