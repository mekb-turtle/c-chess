#include "chess.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char piece_to_letter(struct piece piece) {
	char letter = '-';
	switch (piece.type) {
		case PAWN:
			letter = 'P';
			break;
		case KNIGHT:
			letter = 'N';
			break;
		case BISHOP:
			letter = 'B';
			break;
		case ROOK:
			letter = 'R';
			break;
		case QUEEN:
			letter = 'Q';
			break;
		case KING:
			letter = 'K';
			break;
		default:
			return letter;
	}
	if (piece.color == BLACK) {
		letter = letter + 'a' - 'A';
	}
	return letter;
}

struct piece letter_to_piece(char letter) {
	struct piece piece = {NONE, WHITE};
	if (letter >= 'a' && letter <= 'z') {
		piece.color = BLACK;
		letter = letter - 'a' + 'A';
	}
	switch (letter) {
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

char rank_to_letter(uint8_t rank) {
	return '0' + (CHESS_BOARD_HEIGHT - rank);
}

char file_to_letter(uint8_t file) {
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

static struct position get_direction(struct position from, struct position to) {
	struct position delta = {to.x - from.x, to.y - from.y};
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

bool position_equal(struct position a, struct position b) {
	return a.x == b.x && a.y == b.y;
}

bool position_valid(struct position pos) {
	return pos.x >= 0 && pos.x < CHESS_BOARD_WIDTH && pos.y >= 0 && pos.y < CHESS_BOARD_HEIGHT;
}

struct piece *get_piece(struct game *game, struct position pos) {
	if (!position_valid(pos)) return NULL;
	return &game->board[pos.y][pos.x];
}

bool loop_pieces_between(struct game *game, struct position from, struct position to, bool (*callback)(struct position, void *), void *data) {
	struct position direction = get_direction(from, to);
	if (direction.x == 0 && direction.y == 0) return false;
	while (!position_equal(from, to)) {
		from.x += direction.x;
		from.y += direction.y;
		if (!position_valid(from)) return false;
		if (!callback(from, data)) return false;
	}
	return true;
}

bool loop_board(struct game *game, bool (*callback)(struct position, void *), void *data) {
	for (uint8_t y = 0; y < CHESS_BOARD_HEIGHT; y++) {
		for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
			if (!callback((struct position){x, y}, data)) return false;
		}
	}
	return true;
}

struct move_state_data {
	struct game *game;
	struct move_state *state;
	enum color player;
};

static bool get_move_state_callback(struct position pos, struct piece *piece, void *data) {
	if (piece->type == NONE) return true;
	struct move_state_data move_data = *(struct move_state_data *) data;
	struct move_list *moves = get_legal_moves(move_data.game, pos);
	if (piece->color == move_data.player) {
		if (moves) {
			// if the player has any legal moves, the game is not in stalemate
			move_data.state->stalemate = false;
			if (move_data.state->check) return false; // nothing else to change, exit early
		}
	} else {
		// check if the player is in check
		for (struct move_list *move = moves; move; move = move->next) {
			struct piece *to = &move_data.game->board[move->move.to.y][move->move.to.x];
			if (to->color != move_data.player) continue;
			if (to->type != KING) continue;
			// if the opponent can take the player's king, the player is in check
			move_data.state->check = true;
			if (!move_data.state->stalemate) return false; // nothing else to change, exit early
		}
	}
	return true;
}

struct move_state get_move_state(struct game *game, enum color player) {
	// set the default state to stalemate
	struct move_state state = {.stalemate = true, .check = false};
	struct move_state_data data = {game, &state, player};
	loop_board(game, get_move_state_callback, &data);
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

static void search_moves(struct game *game, struct move_list *list, struct position pos, bool cardinal, bool diagonal, uint8_t max_length) {
	// combine arrays into one
	struct position directions[8];
	memset(directions, 0, sizeof(directions));
	if (cardinal) memcpy(directions, (struct position[4]){

		                                     {0,  1 },
		                                     {1,  0 },
		                                     {0,  -1},
		                                     {-1, 0 }
 },
		                 sizeof(struct position) * 4);
	if (diagonal) memcpy(directions + 4, (struct position[4]){
		                                         {1,  1 },
		                                         {1,  -1},
		                                         {-1, -1},
		                                         {-1, 1 }
 },
		                 sizeof(struct position) * 4);
	// loop all directions
	for (uint8_t i = 0; i < sizeof(directions) / sizeof(directions[0]); ++i) {
		struct position direction = directions[i];
		if (direction.x == 0 && direction.y == 0) continue;
		struct position new_pos = pos;
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

struct move_list *get_legal_moves(struct game *game, struct position pos) {
	struct move_list *list = alloc_move(game);
	struct piece *piece = &game->board[pos.y][pos.x];
	if (piece->type == NONE || piece->color != game->active_color) return list;
	switch (piece->type) {
		case PAWN:;
			int8_t direction = piece->color == WHITE ? 1 : -1;
			if ()
				break;
		case KNIGHT:;
			struct position directions[8] = {
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
				add_move(game, list, (struct move){
				                             .from = pos,
				                             .to = {pos.x + directions[i].x, pos.y + directions[i].y}
                });
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
	return list;
}

#include <stdio.h>

void chess_print(struct game *game) {
	printf("  ");
	for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
		printf("%c ", file_to_letter(x));
	}
	printf("\n");
	for (uint8_t y = 0; y < CHESS_BOARD_HEIGHT; y++) {
		printf("%c ", rank_to_letter(y));
		for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
			struct piece *p = &game->board[y][x];
			printf("%c ", piece_to_letter(*p));
		}
		printf("\n");
	}
}
