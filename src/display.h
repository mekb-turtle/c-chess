#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdbool.h>
#include <stdio.h>
#include "chess.h"

struct display_settings {
	bool unicode, color, view_flip, extra_space;
};
void print_colored(struct display_settings display, enum piece_color color, char *str, FILE *fp);
void print_color(struct display_settings display, enum piece_color color, FILE *fp);
void print_bool(struct display_settings display, bool state, FILE *fp);
void print_board(struct display_settings display, struct game *game, FILE *fp);
#endif
