#include "chess.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char piece_to_char(struct piece piece) {
	char c = '-';
	switch (piece.type) {
		case PAWN:
			c = 'P';
			break;
		case KNIGHT:
			c = 'N';
			break;
		case BISHOP:
			c = 'B';
			break;
		case ROOK:
			c = 'R';
			break;
		case QUEEN:
			c = 'Q';
			break;
		case KING:
			c = 'K';
			break;
		default:
			return c;
	}
	if (piece.color == BLACK) {
		c = c + 'a' - 'A';
	}
	return c;
}

struct piece char_to_piece(char c) {
	struct piece piece = {NONE, WHITE};
	if (c >= 'a' && c <= 'z') {
		piece.color = BLACK;
		c = c - 'a' + 'A';
	}
	switch (c) {
		case 'P':
			piece.type = PAWN;
			break;
		case 'N':
			piece.type = KNIGHT;
			break;
		case 'B':
			piece.type = BISHOP;
			break;
		case 'R':
			piece.type = ROOK;
			break;
		case 'Q':
			piece.type = QUEEN;
			break;
		case 'K':
			piece.type = KING;
			break;
	}
	return piece;
}

char rank_to_char(uint8_t rank) {
	return '1' + rank;
}

char file_to_char(uint8_t file) {
	return 'a' + file;
}

void chess_init(struct game *game) {
	game->active_color = WHITE;
	game->castle.white.king = true;
	game->castle.white.queen = true;
	game->castle.black.king = true;
	game->castle.black.queen = true;
	game->en_passant.x = 0;
	game->en_passant.y = 0;
	game->halfmove = 0;
	game->fullmove = 1;
	for (uint8_t y = 0; y < CHESS_BOARD_HEIGHT; y++) {
		for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
			game->board[y][x].type = NONE;
			if (y == 1 || y == CHESS_BOARD_HEIGHT - 2) {
				game->board[y][x].type = PAWN;
			} else if (y == 0 || y == CHESS_BOARD_HEIGHT - 1) {
				switch (x) {
					case 0:
					case CHESS_BOARD_WIDTH - 1:
						game->board[y][x].type = ROOK;
						break;
					case 1:
					case CHESS_BOARD_WIDTH - 2:
						game->board[y][x].type = KNIGHT;
						break;
					case 2:
					case CHESS_BOARD_WIDTH - 3:
						game->board[y][x].type = BISHOP;
						break;
					case 3:
						game->board[y][x].type = QUEEN;
						break;
					case CHESS_BOARD_WIDTH - 4:
						game->board[y][x].type = KING;
						break;
				}
			}
			game->board[y][x].color = y > CHESS_BOARD_HEIGHT / 2 ? BLACK : WHITE;
		}
	}
}

static int8_t sign(int8_t x) {
	if (x > 0) return 1;
	if (x < 0) return -1;
	return 0;
}

static position get_direction(position from, position to) {
	position delta = {to.x - from.x, to.y - from.y};
	// no direction if the positions are the same
	if (delta.x == 0 && delta.y == 0) goto none;
	// invalid if positions are not on the same rank, file, or diagonal
	if (delta.x != 0 && delta.y != 0)
		if (delta.x != delta.y && delta.x != -delta.y) goto none;
	// normalize the direction
	delta.x = sign(delta.x);
	delta.y = sign(delta.y);
	return delta;
none:
	delta.x = 0;
	delta.y = 0;
	return delta;
}

bool position_equal(position a, position b) {
	return a.x == b.x && a.y == b.y;
}

bool position_valid_xy(int8_t x, int8_t y) {
	return x >= 0 && x < CHESS_BOARD_WIDTH && y >= 0 && y < CHESS_BOARD_HEIGHT;
}

bool position_valid(position pos) {
	return position_valid_xy(pos.x, pos.y);
}

struct piece *get_piece_xy(struct game *game, int8_t x, int8_t y) {
	if (!position_valid_xy(x, y)) return NULL;
	return &game->board[y][x];
}

struct piece *get_piece(struct game *game, position pos) {
	return get_piece_xy(game, pos.x, pos.y);
}

bool loop_pieces_between(position from, position to, bool (*callback)(position, void *), void *data) {
	position direction = get_direction(from, to);
	if (direction.x == 0 && direction.y == 0) return false;
	while (!position_equal(from, to)) {
		from.x += direction.x;
		from.y += direction.y;
		if (!position_valid(from)) return false;
		if (!callback(from, data)) return false;
	}
	return true;
}

bool loop_board(bool (*callback)(position, void *), void *data) {
	for (uint8_t y = 0; y < CHESS_BOARD_HEIGHT; y++) {
		for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
			if (!callback(POS(x, y), data)) return false;
		}
	}
	return true;
}

struct move_state_data {
	struct game *game;
	struct move_state *state;
	enum color player;
};

static bool get_move_state_callback(position pos, void *data_) {
	struct move_state_data data = *(struct move_state_data *) data_;
	struct piece *piece = get_piece(data.game, pos);
	if (piece->type == NONE) return true;
	struct move_list *moves = get_legal_moves(data.game, pos);
	if (piece->color == data.player) {
		if (moves) {
			// if the player has any legal moves, the game is not in stalemate
			data.state->stalemate = false;
			if (data.state->check) return false; // nothing else to change, exit early
		}
	} else {
		// check if the player is in check
		for (struct move_list *move = moves; move; move = move->next) {
			struct piece *to = &data.game->board[move->move.to.y][move->move.to.x];
			if (to->color != data.player) continue;
			if (to->type != KING) continue;
			// if the opponent can take the player's king, the player is in check
			data.state->check = true;
			if (!data.state->stalemate) return false; // nothing else to change, exit early
		}
	}
	return true;
}

struct move_state get_move_state(struct game *game, enum color player) {
	// set the default state to stalemate
	struct move_state state = {.stalemate = true, .check = false};
	struct move_state_data data = {game, &state, player};
	loop_board(get_move_state_callback, &data);
	return state;
}

static struct move_list *alloc_move(struct game *game) {
	struct move_list *new = game->malloc(sizeof(struct move_list));
	if (!new) {
		perror("malloc");
		exit(1);
	}
	return new;
}

void add_move(struct game *game, struct move_list *list, struct move move) {
	// add the move to the end of the list
	struct move_list *new = alloc_move(game);
	new->move = move;

	// insert new node after the current node
	// does not need to be at the end of the list since order does not matter
	new->next = list->next;
	list->next = new;

	list = new; // move the list pointer to the new node
}

static void search_moves(struct game *game, struct move_list *list, position pos, bool cardinal, bool diagonal, uint8_t max_length) {
	// combine arrays into one
	position directions[8];
	memset(directions, 0, sizeof(directions));
	if (cardinal) {
		directions[0] = POS(0, 1);
		directions[1] = POS(1, 0);
		directions[2] = POS(0, -1);
		directions[3] = POS(-1, 0);
	}
	if (diagonal) {
		directions[4] = POS(1, 1);
		directions[5] = POS(1, -1);
		directions[6] = POS(-1, -1);
		directions[7] = POS(-1, 1);
	}
	// loop all directions
	for (uint8_t i = 0; i < sizeof(directions) / sizeof(directions[0]); ++i) {
		position direction = directions[i];
		if (direction.x == 0 && direction.y == 0) continue;
		position new_pos = pos;
		for (uint8_t distance = 0; max_length == 0 || distance < max_length; ++distance) {
			new_pos.x += direction.x;
			new_pos.y += direction.y;
			if (!position_valid(new_pos)) break;

			struct piece *piece = &game->board[new_pos.y][new_pos.x];
			struct move move = {.from = pos, .to = new_pos};
			if (piece->type != NONE) {
				// add piece if it is an opponent piece
				if (piece->color != game->active_color) {
					add_move(game, list, move);
				}
				// stop searching if there is a piece in the way
				break;
			}
			add_move(game, list, move);
		}
	}
}

struct move_list *get_legal_moves(struct game *game, position pos) {
	struct move_list *list = alloc_move(game); // dummy node to simplify adding moves to the list
	struct piece *piece = &game->board[pos.y][pos.x];
	if (piece->type == NONE || piece->color != game->active_color) return list;
	switch (piece->type) {
		case PAWN:;
			int8_t direction = piece->color == WHITE ? 1 : -1;
			int8_t home_rank = piece->color == WHITE ? 1 : CHESS_BOARD_HEIGHT - 2;

			position forward = POS(pos.x, pos.y + direction);
			position forward2 = POS(pos.x, pos.y + 2 * direction);
			struct piece *forward_piece = get_piece(game, forward);
			struct piece *forward2_piece = get_piece(game, forward2);
			if (forward_piece && forward_piece->type == NONE) {
				// pawn can move forward
				add_move(game, list, MOVE(pos, forward, ));
				if (home_rank == pos.y && forward2_piece && forward2_piece->type == NONE) {
					// pawn can move forward two spaces if it is on its home rank
					add_move(game, list, MOVE(pos, forward2, ));
				}
			}

			position left = POS(pos.x - 1, pos.y + direction);
			position right = POS(pos.x + 1, pos.y + direction);
			struct piece *left_piece = get_piece(game, left);
			struct piece *right_piece = get_piece(game, right);
			if (left_piece) {
				if (left_piece->type != NONE && left_piece->color != piece->color) {
					add_move(game, list, MOVE(pos, left, ));
				} else if (position_equal(game->en_passant, left)) {
					add_move(game, list, MOVE(pos, left, .en_passant = true));
				}
			}
			if (right_piece) {
				if (right_piece->type != NONE && right_piece->color != piece->color) {
					add_move(game, list, MOVE(pos, right, ));
				} else if (position_equal(game->en_passant, right)) {
					add_move(game, list, MOVE(pos, right, .en_passant = true));
				}
			}
			break;
		case KNIGHT:;
			position directions[8] = {
			        {1,  2 },
			        {2,  1 },
			        {2,  -1},
			        {1,  -2},
			        {-1, -2},
			        {-2, -1},
			        {-2, 1 },
			        {-1, 2 }
            };
			for (uint8_t i = 0; i < sizeof(directions) / sizeof(directions[0]); ++i) {
				add_move(game, list, MOVE(pos, POS(pos.x + directions[i].x, pos.y + directions[i].y), ));
			}
			break;
		case BISHOP:
			search_moves(game, list, pos, false, true, 0);
			break;
		case ROOK:
			search_moves(game, list, pos, true, false, 0);
			break;
		case QUEEN:
			search_moves(game, list, pos, true, true, 0);
			break;
		case KING:
			search_moves(game, list, pos, true, true, 1);
			break;
		default:
			break;
	}
	return list->next; // skip the dummy node
}

#include <stdio.h>

void chess_print(struct game *game) {
	printf("  ");
	for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
		printf("%c ", file_to_char(x));
	}
	printf("\n");
	for (uint8_t y_ = 0; y_ < CHESS_BOARD_HEIGHT; y_++) {
		uint8_t y = CHESS_BOARD_HEIGHT - 1 - y_;
		printf("%c ", rank_to_char(y));
		for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
			struct piece *p = get_piece(game, POS(x, y));
			if (p->type == NONE)
				printf("%c ", piece_to_char(*p));
			else if (p->color == WHITE)
				printf("\033[1;47;30m%c\033[0m ", piece_to_char(*p));
			else
				printf("\033[1;40;37m%c\033[0m ", piece_to_char(*p));
		}
		printf("\n");
	}
}
