#include "input.h"
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>

#include "display.h"

int scan_char(FILE *fp, bool blocking) {
	// get old terminal settings
	struct termios old, new;
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

	int fl = 0;
	fl = fcntl(fileno(fp), F_GETFL);
	if (fl == -1) {
		perror("fcntl");
		exit(2);
	}
	int new_fl = fl;
	if (blocking)
		new_fl &= ~O_NONBLOCK;
	else
		new_fl |= O_NONBLOCK;
	if (fcntl(fileno(fp), F_SETFL, new_fl) == -1) {
		perror("fcntl");
		exit(2);
	}

	int c = EOF;

	c = fgetc(fp);
	fflush(fp);

	if (tcsetattr(fileno(fp), TCSANOW, &old)) {
		perror("tcsetattr");
		exit(2);
	}
	if (fcntl(fileno(fp), F_SETFL, fl) == -1) {
		perror("fcntl");
		exit(2);
	}
	return c;
}

struct move prompt_for_move(struct display_settings display, struct game *game, FILE *out, FILE *in, bool *view_flip, void (*print)(struct game *)) {
	const size_t str_len = 16;
	char str[str_len];
	memset(str, 0, sizeof(str));

reprint_move:
	fprintf(out, "Enter move: (");
	print_color(display, game->active_color, stdout);
	fprintf(out, ")\n");
	while (true) {
		// clear line
		fprintf(out, "\x1b[G");
		struct move move;
		enum find_move_reason reason = find_move(game, &move, str);
		if (display.color) {
			// provide color feedback
			int color = 0;
			switch (reason) {
				case REASON_NONE:
					color = 42;
					break;
				case REASON_WIN:
					color = 44;
					break;
				case REASON_AMBIGUOUS:
					color = 43;
					break;
				case REASON_ILLEGAL:
					color = 45;
					break;
				case REASON_SYNTAX:
					color = 41;
					break;
				case REASON_NONE_FOUND:
					color = 46;
					break;
			}
			fprintf(out, "\x1b[0;%im", color);
		}
		fprintf(out, "> ");
		if (display.color)
			fprintf(out, "\x1b[0m");
		fprintf(out, "%s", str);
		size_t len = strlen(str);
		// padding
		fprintf(out, "\x1b[s");
		for (size_t i = 0; i < str_len - len; ++i) fprintf(out, " ");
		fprintf(out, "\x1b[u");

		// input
		int c = scan_char(in, true);
		if (c == '\t') {
			*view_flip = !*view_flip;
			fprintf(out, "\x1b[G%*s\x1b[G", (int) str_len, "");
			print(game);
			continue;
		} else if (c == '\n' || c == '\r') {
			if (reason == REASON_NONE) {
				fprintf(out, "\n");
				return move;
			}
			if (display.color)
				fprintf(out, "\x1b[0m");
			fprintf(out, "\x1b[G%*s\x1b[GInvalid move, ", (int) str_len, "");
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
