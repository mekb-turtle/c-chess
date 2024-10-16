#include "display.h"
#include <string.h>

static bool print_line(char **line, FILE *fp) {
	const size_t limit = 100;
	if (!line) return false;
	if (!*line) return false;
	size_t max_len = 0, i = 1;
	for (i = 1; (*line)[i] && i < limit; ++i) {
		// split at double spaces
		if ((*line)[i - 1] == ' ')
			if ((*line)[i] == ' ')
				max_len = i;
	}
	if (!max_len || i < limit) max_len = i;
	if (max_len) {
		fwrite(*line, sizeof(char), max_len, fp);
		*line = *line + max_len;
	}
	for (; **line == ' '; ++*line); // skip leading spaces
	if (!**line) {                  // end of string
		*line = NULL;
		return false;
	}
	return true;
}

void print_colored(struct display_settings display, enum piece_color color, char *str, FILE *fp) {
	if (!display.color) {
		fprintf(fp, "%s", str);
		return;
	}
	switch (color) {
		case COLOR_WHITE:
			fprintf(fp, "\033[1;47;30m%s\033[0m", str);
			break;
		case COLOR_BLACK:
			fprintf(fp, "\033[1;40;37m%s\033[0m", str);
			break;
	}
}

void print_color(struct display_settings display, enum piece_color color, FILE *fp) {
	print_colored(display, color, color == COLOR_WHITE ? "White" : "Black", fp);
}

void print_bool(struct display_settings display, bool state, FILE *fp) {
	if (!display.color) {
		fprintf(fp, "%s", state ? "Yes" : "No");
		return;
	}
	if (state)
		fprintf(fp, "\033[1;42;30mYes\033[0m");
	else
		fprintf(fp, "\033[1;41;30mNo\033[0m");
}

void print_board(struct display_settings display, bool flip, struct game *game, FILE *fp) {
	char *move_str = get_move_string(game);
	char *whole_move_str = move_str;
	fprintf(fp, "  ");
	for (uint8_t x_ = 0; x_ < CHESS_BOARD_WIDTH; x_++) {
		uint8_t x = flip ? CHESS_BOARD_WIDTH - 1 - x_ : x_;
		fprintf(fp, "%c ", file_to_char(x));
	}
	print_line(&move_str, fp);
	fprintf(fp, "\n");
	for (uint8_t y_ = 0; y_ < CHESS_BOARD_HEIGHT; y_++) {
		// flip if necessary
		uint8_t y = flip ? y_ : CHESS_BOARD_HEIGHT - 1 - y_;
		fprintf(fp, "%c ", rank_to_char(y));
		for (uint8_t x_ = 0; x_ < CHESS_BOARD_WIDTH; x_++) {
			uint8_t x = flip ? CHESS_BOARD_WIDTH - 1 - x_ : x_;
			struct piece *p = get_piece(game, POS(x, y));
			char c[6];
			memset(c, 0, sizeof(c));
			if (!display.unicode) {
				if (p->type == TYPE_NONE)
					c[0] = '-';
				else
					c[0] = piece_to_char_struct(*p);
			} else {
				if (p->type == TYPE_NONE) {
					// dot
					c[0] = '\xc2';
					c[1] = '\xb7';
				} else {
					// chess pieces, pieces are filled if color is enabled or if the piece is black
					c[0] = '\xe2';
					c[1] = '\x99';
					c[2] = '\x93' + p->type + (p->color == COLOR_BLACK || display.color ? 6 : 0);
				}
			}
			if (p->type == TYPE_NONE)
				fprintf(fp, "%s", c);
			else
				print_colored(display, p->color, c, fp);
			fprintf(fp, " ");
		}
		print_line(&move_str, fp);
		fprintf(fp, "\n");
	}
	game->free(whole_move_str);
}
