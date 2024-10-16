#ifndef INPUT_H
#define INPUT_H
#include <stdbool.h>
#include <stdio.h>

#include "chess.h"
#include "display.h"

int scan_char(FILE *fp, bool blocking);
struct move prompt_for_move(struct display_settings display, struct game *game, FILE *out, FILE *in, bool *view_flip, void (*print)(struct game *));
#endif
