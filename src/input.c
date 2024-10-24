#include "input.h"
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>

#include "display.h"
#include "chess.h"

static bool is_input = false;
static struct termios old;
static int fl = 0;

void input_exit(FILE *fp) {
	if (!is_input) return;
	if (fcntl(fileno(fp), F_SETFL, fl) == -1) {
		perror("fcntl");
	}
	if (tcsetattr(fileno(fp), TCSANOW, &old)) {
		perror("tcsetattr");
	}
	is_input = false;
}

int scan_char(FILE *fp, bool blocking) {
	bool exit_ = false;
	// get old terminal settings
	struct termios new;
	if (tcgetattr(fileno(fp), &old)) {
		perror("tcgetattr");
		exit(2);
	}
	new = old;
	// set new terminal settings
	new.c_lflag &= ~ICANON; // disable canonical mode
	new.c_lflag &= ~ECHO;   // disable echo back of input
	if (tcsetattr(fileno(fp), TCSANOW, &new)) {
		perror("tcsetattr");
		exit(2);
	}

	fl = fcntl(fileno(fp), F_GETFL);
	if (fl == -1) {
		perror("fcntl");
		exit_ = true;
		goto tcsetattr;
	}
	int new_fl = fl;
	if (blocking)
		new_fl &= ~O_NONBLOCK;
	else
		new_fl |= O_NONBLOCK;
	if (fcntl(fileno(fp), F_SETFL, new_fl) == -1) {
		perror("fcntl");
		exit_ = true;
		goto tcsetattr;
	}
	is_input = true;

	int c = EOF;

	c = fgetc(fp);
	fflush(fp);

	if (fcntl(fileno(fp), F_SETFL, fl) == -1) {
		perror("fcntl");
		exit_ = true;
	}
tcsetattr:
	if (tcsetattr(fileno(fp), TCSANOW, &old)) {
		perror("tcsetattr");
		exit_ = true;
	}
	if (exit_) {
		is_input = false;
		exit(2);
	}
	return c;
}

struct move prompt_for_move(struct display_settings display, struct game *game, FILE *out, FILE *in, bool *view_flip, void (*print)(struct game *)) {
	const size_t str_len = 10;
	char str[str_len];
	memset(str, 0, sizeof(str));

reprint_move:
	fprintf(out, "Enter move: (");
	print_color(display, game->active_color, stdout);
	fprintf(out, ") ");
	// print move number
	fprintf(out, "%lu.", game->full_move);
	if (game->active_color == COLOR_BLACK) fprintf(out, "..");
	fprintf(out, "\n");
	while (true) {
		// clear line
		fprintf(out, "\x1b[G");
		struct move move;
		enum find_move_reason reason = find_move(game, &move, str);
		if (display.color) {
			// provide color feedback
			int color;
			switch (reason) {
				case REASON_SYNTAX:
					color = 44;
					break;
				case REASON_SUCCESS:
					color = 42;
					break;
				case REASON_AMBIGUOUS:
					color = 43;
					break;
				default:
					color = 41;
					break;
			}
			fprintf(out, "\x1b[1;%i;30m", color);
		}
		fprintf(out, ">");
		if (display.color)
			fprintf(out, "\x1b[0m");
		fprintf(out, " ");
		// print string
		fprintf(out, "%s", str);
		size_t len = strlen(str);
		size_t clear_len = str_len - len + 2; // extra for luck
		// padding
		for (size_t i = 0; i < clear_len; ++i) fprintf(out, " ");
		for (size_t i = 0; i < clear_len; ++i) fprintf(out, "\x1b[D");

		// input
		int c = scan_char(in, true);
		if (c == '\t') {
			*view_flip = !*view_flip;
			if (display.color)
				fprintf(out, "\x1b[0m");
			fprintf(out, "\x1b[G%*s\x1b[%iF%*s\x1b[G", (int) str_len, "", 3 + CHESS_BOARD_HEIGHT, 30, "");
			print(game);
			goto reprint_move;
		} else if (c == '\n' || c == '\r') {
			if (reason == REASON_SUCCESS) {
				fprintf(out, "\n");
				return move;
			}
			if (display.color)
				fprintf(out, "\x1b[0m");
			fprintf(out, "\x1b[G%*s\x1b[F%*s\x1b[G", (int) str_len, "", 30, "");
			switch (reason) {
				case REASON_AMBIGUOUS:
					fprintf(out, "Ambiguous move");
					break;
				case REASON_ILLEGAL:
					fprintf(out, "Illegal move");
					break;
				default:
					fprintf(out, "Invalid move");
					break;
			}
			fprintf(out, ", ");
			goto reprint_move;
		} else if (c == 127 || c == 8) {
			// backspace
			if (len)
				str[len - 1] = '\0';
			continue;
		} else if (len < str_len - 1 && c >= ' ' && c <= '~') {
			str[len] = c;
			str[len + 1] = '\0';
		}
	}
}
