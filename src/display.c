#include "display.h"
#include <string.h>

static bool print_line(struct display_settings display, char **line, FILE *fp) {
	const size_t limit = 100;
	if (!line) return false;
	if (!*line) return false;
	if (!**line) {
		*line = NULL;
		return false;
	}
	size_t max_len = 0, i = 1;
	for (i = 1; (*line)[i] && i < limit; ++i) {
		// split at double spaces
		if ((*line)[i - 1] == ' ')
			if ((*line)[i] == ' ')
				max_len = i;
	}
	if (i < limit) max_len = i; // end of string
	else if (!max_len) max_len = i - 1;
	if (max_len) {
		if (display.color) {
			fprintf(fp, "\x1b[1;47;30m \x1b[0m");
		} else {
			fprintf(fp, "|");
		}
		fprintf(fp, " ");
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
			fprintf(fp, "\x1b[1;47;30m%s\x1b[0m", str);
			break;
		case COLOR_BLACK:
			fprintf(fp, "\x1b[1;40;37m%s\x1b[0m", str);
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
		fprintf(fp, "\x1b[1;42;30mYes\x1b[0m");
	else
		fprintf(fp, "\x1b[1;41;30mNo\x1b[0m");
}

static void print_piece(struct display_settings display, struct piece p, FILE *fp) {
	char c[6];
	memset(c, 0, sizeof(c));
	if (!display.unicode) {
		if (p.type == TYPE_NONE)
			c[0] = '-';
		else
			c[0] = piece_to_char_struct(p);
	} else {
		if (p.type == TYPE_NONE) {
			// dot
			c[0] = '\xc2';
			c[1] = '\xb7';
		} else {
			// chess pieces, pieces are filled if color is enabled or if the piece is black
			c[0] = '\xe2';
			c[1] = '\x99';
			c[2] = '\x93' + p.type + (p.color == COLOR_BLACK || display.color ? 6 : 0);
		}
	}
	if (display.extra_space) c[strlen(c)] = ' ';
	if (p.type == TYPE_NONE)
		fprintf(fp, "%s", c);
	else
		print_colored(display, p.color, c, fp);
}

static void print_files(struct display_settings display, char **line, FILE *fp) {
	fprintf(fp, "  ");
	for (uint8_t x_ = 0; x_ < CHESS_BOARD_WIDTH; x_++) {
		uint8_t x = display.view_flip ? CHESS_BOARD_WIDTH - 1 - x_ : x_;
		fprintf(fp, "%c ", file_to_char(x));
		if (display.extra_space) fprintf(fp, " ");
	}
	fprintf(fp, "  ");
	print_line(display, line, fp);
	fprintf(fp, "\n");
}

void print_board(struct display_settings display, struct game *game, FILE *fp) {
	char *move_str = get_move_string(game);
	char *whole_move_str = move_str;

	print_files(display, &move_str, fp);
	for (uint8_t y_ = 0; y_ < CHESS_BOARD_HEIGHT; y_++) {
		// flip if necessary
		uint8_t y = display.view_flip ? y_ : CHESS_BOARD_HEIGHT - 1 - y_;
		fprintf(fp, "%c ", rank_to_char(y));
		for (uint8_t x_ = 0; x_ < CHESS_BOARD_WIDTH; x_++) {
			uint8_t x = display.view_flip ? CHESS_BOARD_WIDTH - 1 - x_ : x_;
			struct piece *p = get_piece(game, POS(x, y));
			print_piece(display, *p, fp);
			fprintf(fp, " ");
		}
		fprintf(fp, "%c ", rank_to_char(y));
		print_line(display, &move_str, fp);
		fprintf(fp, "\n");
	}
	print_files(display, &move_str, fp);
	game->free(whole_move_str);
}
