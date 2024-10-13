#include "chess.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char piece_to_char(enum piece_type type, bool lowercase) {
	char c = '\x00';
	switch (type) {
		case TYPE_PAWN:
			c = 'P';
			break;
		case TYPE_KNIGHT:
			c = 'N';
			break;
		case TYPE_BISHOP:
			c = 'B';
			break;
		case TYPE_ROOK:
			c = 'R';
			break;
		case TYPE_QUEEN:
			c = 'Q';
			break;
		case TYPE_KING:
			c = 'K';
			break;
		default:
			return c;
	}
	if (lowercase) {
		c = c + 'a' - 'A';
	}
	return c;
}

char piece_to_char_struct(struct piece piece) {
	return piece_to_char(piece.type, piece.color == COLOR_BLACK);
}

struct piece char_to_piece(char c) {
	struct piece piece = {TYPE_NONE, COLOR_WHITE};
	if (c >= 'a' && c <= 'z') {
		piece.color = COLOR_BLACK;
		c = c - 'a' + 'A';
	}
	switch (c) {
		case 'P':
			piece.type = TYPE_PAWN;
			break;
		case 'N':
			piece.type = TYPE_KNIGHT;
			break;
		case 'B':
			piece.type = TYPE_BISHOP;
			break;
		case 'R':
			piece.type = TYPE_ROOK;
			break;
		case 'Q':
			piece.type = TYPE_QUEEN;
			break;
		case 'K':
			piece.type = TYPE_KING;
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

enum piece_color get_opposite_color(enum piece_color color) {
	return color == COLOR_WHITE ? COLOR_BLACK : COLOR_WHITE;
}

struct game *create_board(void *(*malloc_)(size_t), void (*free_)(void *)) {
	struct game *game = malloc_(sizeof(struct game));
	if (!game) {
		perror("malloc");
		exit(1);
	}
	game->malloc = malloc_;
	game->free = free_;
	board_init(game);
	return game;
}

void destroy_board(struct game *game) {
	if (!game) return;

	free_move_list(game, game->move_list);

	void (*free_)(void *) = game->free;
	free_(game);
}

void board_init(struct game *game) {
	game->win = STATE_NONE;
	// white starts first
	game->active_color = COLOR_WHITE;
	// all castling is allowed at the start
	game->castle_availability[COLOR_WHITE] = GAME_CASTLE_ALL;
	game->castle_availability[COLOR_BLACK] = GAME_CASTLE_ALL;
	// reset en passant target
	game->en_passant_target.x = 0;
	game->en_passant_target.y = 0;
	// reset counters
	game->half_move = 0;
	game->full_move = 1;

	// initialize the board
	struct position pos;
	for (pos.y = 0; pos.y < CHESS_BOARD_HEIGHT; pos.y++) {
		for (pos.x = 0; pos.x < CHESS_BOARD_WIDTH; pos.x++) {
			struct piece *piece = get_piece(game, pos);
			if (!piece) continue;
			piece->type = TYPE_NONE;
			if (pos.y == 1 || pos.y == CHESS_BOARD_HEIGHT - 2) {
				piece->type = TYPE_PAWN;
			} else if (pos.y == 0 || pos.y == CHESS_BOARD_HEIGHT - 1) {
				switch (pos.x) {
					case 0:
					case CHESS_BOARD_WIDTH - 1:
						piece->type = TYPE_ROOK;
						break;
					case 1:
					case CHESS_BOARD_WIDTH - 2:
						piece->type = TYPE_KNIGHT;
						break;
					case 2:
					case CHESS_BOARD_WIDTH - 3:
						piece->type = TYPE_BISHOP;
						break;
					case 3:
						piece->type = TYPE_QUEEN;
						break;
					case CHESS_BOARD_WIDTH - 4:
						piece->type = TYPE_KING;
						break;
				}
			}
			piece->color = pos.y > CHESS_BOARD_HEIGHT / 2 ? COLOR_BLACK : COLOR_WHITE;
		}
	}
}

static int8_t abs8(int8_t x) {
	return x < 0 ? -x : x;
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

bool position_valid_xy(int8_t x, int8_t y) {
	return x >= 0 && x < CHESS_BOARD_WIDTH && y >= 0 && y < CHESS_BOARD_HEIGHT;
}

bool position_valid(struct position pos) {
	return position_valid_xy(pos.x, pos.y);
}

struct piece *get_piece_xy(struct game *game, int8_t x, int8_t y) {
	if (!position_valid_xy(x, y)) return NULL;
	return &game->board[y][x];
}

struct piece *get_piece(struct game *game, struct position pos) {
	return get_piece_xy(game, pos.x, pos.y);
}

static bool loop_pieces_between(struct position from, struct position to, bool (*callback)(struct position, struct game *, struct position from, struct position to, void *), struct game *game, void *data) {
	struct position pos = from;
	struct position direction = get_direction(from, to);
	if (direction.x == 0 && direction.y == 0) return false;
	while (!position_equal(pos, to)) {
		if (!position_valid(pos)) return false;
		if (!callback(pos, game, from, to, data)) return false;
		pos.x += direction.x;
		pos.y += direction.y;
	}
	if (!position_valid(pos)) return false;
	return callback(pos, game, from, to, data); // pos == to
}

// shorthand function
static bool match_piece(struct piece *piece, enum piece_type type, enum piece_color color) {
	return piece && piece->type == type && piece->color == color;
}

static struct move_list *get_available_moves_internal(struct game *game, enum piece_color player, bool check_threat);
static bool perform_move_internal(struct game *game, struct move move);

static bool get_if_check(struct game *game, enum piece_color player) {
	// check if the player is in check
	for (struct move_list *moves = get_available_moves_internal(game, get_opposite_color(player), true)->next;
	     moves; moves = moves->next) {
		if (moves->move.type == MOVE_CASTLE) continue;
		struct piece *to = get_piece(game, moves->move.to);
		if (!match_piece(to, TYPE_KING, player)) continue;
		// if the opponent can take the player's king, the player is in check
		return true;
	}
	return false;
}

struct move_state get_move_state(struct game *game, enum piece_color player) {
	struct move_state state = {.check = false};
	// if the player has no legal moves, the game is in stalemate
	state.stalemate = !get_available_moves_internal(game, player, false)->next;
	// check if the player is in check
	state.check = get_if_check(game, player);
	// checkmate occurs if the player is in check and has no legal moves
	return state;
}

static struct move_list *alloc_move(struct game *game) {
	struct move_list *new = game->malloc(sizeof(struct move_list));
	if (!new) {
		perror("malloc");
		exit(1);
	}
	new->next = NULL;
	return new;
}

struct move_list *add_move(struct game *game, struct move_list *list, struct move move) {
	// add the move to the end of the list
	struct move_list *new = alloc_move(game);
	new->move = move;

	// insert new node after the current node
	// does not need to be at the end of the list since order does not matter
	new->next = list->next;
	list->next = new;
	return new;
}

static void add_move_end(struct game *game, struct move move) {
	if (game->move_list_tail) {
		game->move_list_tail = add_move(game, game->move_list_tail, move);
		return;
	}
	game->move_list = alloc_move(game);
	game->move_list->move = move;
	game->move_list_tail = game->move_list;
}

static void search_moves(struct game *game, struct move_list *list, struct position pos, bool cardinal, bool diagonal, uint8_t max_length) {
	// combine arrays into one
	struct position directions[8];
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
		struct position direction = directions[i];
		if (direction.x == 0 && direction.y == 0) continue;
		struct position new_pos = pos;
		for (uint8_t distance = 0; max_length == 0 || distance < max_length; ++distance) {
			new_pos.x += direction.x;
			new_pos.y += direction.y;

			struct piece *piece = get_piece(game, new_pos);
			if (!piece) break;

			if (piece->type != TYPE_NONE) {
				// stop searching if there is a same colored piece in the way
				if (piece->color == game->active_color) break;
			}
			add_move(game, list, MOVE(pos, new_pos));

			// stop searching if there is the opponent's piece in the way
			if (piece->type != TYPE_NONE) break;
		}
	}
}

static bool castle_check_empty_callback(struct position pos, struct game *game, struct position from, struct position to, void *data) {
	(void) data;
	// obviously the king and rook positions are not empty
	if (position_equal(pos, from)) return true;
	if (position_equal(pos, to)) return true;

	// every other piece between them must be empty though
	struct piece *piece = get_piece(game, pos);
	if (!piece) return false;
	if (piece->type != TYPE_NONE) return false; // break if there is a piece in the way
	return true;
}

static bool castle_check_no_attack_callback(struct position pos, struct game *game, struct position from, struct position to, void *data) {
	(void) from;
	(void) to;
	struct piece *piece = get_piece(game, pos);
	if (!piece) return false;

	// the tiles the king moves through must not be under attack
	for (struct move_list *moves = get_available_moves_internal(game, get_opposite_color((enum piece_color) data), true)->next;
	     moves; moves = moves->next) {
		if (moves->move.type == MOVE_CASTLE) continue;
		if (position_equal(moves->move.to, pos)) return false;
	}
	return true;
}

static void find_castle_moves(struct game *game, struct move_list *list, enum piece_color player) {
	// get positions of the king and rooks
	struct position king = POS(CHESS_BOARD_WIDTH - 4, player == COLOR_WHITE ? 0 : CHESS_BOARD_HEIGHT - 1);
	struct position rook_king_side = POS(CHESS_BOARD_WIDTH - 1, king.y);
	struct position rook_queen_side = POS(0, king.y);
	// destination positions
	struct position destination_king_side = POS(king.x + 2, king.y);
	struct position destination_queen_side = POS(king.x - 2, king.y);

	// get pieces
	struct piece *king_piece = get_piece(game, king);

	// check if the pieces are the correct type
	if (!match_piece(king_piece, TYPE_KING, player)) return;

	if ((game->castle_availability[player] & GAME_CASTLE_KING_SIDE) == GAME_CASTLE_KING_SIDE)
		// confirm there are no pieces between the king and rook
		if (loop_pieces_between(king, rook_king_side, castle_check_empty_callback, game, (void *) player))
			// confirm the tiles the king moves through are not under attack
			if (loop_pieces_between(king, destination_king_side, castle_check_no_attack_callback, game, (void *) player))
				// add the move to the list
				add_move(game, list, (struct move){.type = MOVE_CASTLE, .castle = KING_SIDE});

	if ((game->castle_availability[player] & GAME_CASTLE_QUEEN_SIDE) == GAME_CASTLE_QUEEN_SIDE)
		// confirm there are no pieces between the king and rook
		if (loop_pieces_between(king, rook_queen_side, castle_check_empty_callback, game, (void *) player))
			// confirm the tiles the king moves through are not under attack
			if (loop_pieces_between(king, destination_queen_side, castle_check_no_attack_callback, game, (void *) player))
				// add the move to the list
				add_move(game, list, (struct move){.type = MOVE_CASTLE, .castle = QUEEN_SIDE});
}

static void filter_moves(struct game *game, struct move_list *list, enum piece_color player, bool (*callback)(struct game *, struct move_list *, enum piece_color, void *), void *data) {
	for (struct move_list *previous = list;; previous = previous->next) {
	next:;
		struct move_list *current = previous->next;
		if (!current) break;
		if (!callback(game, current, player, data)) {
			// remove the move from the list
			previous->next = current->next;
			game->free(current);
			goto next; // avoid skipping the next one because previous->next has already been changed
		}
	}
}

static bool filter_valid_moves(struct game *game, struct move_list *list, enum piece_color player, void *data) {
	(void) data;
	struct move *move = &list->move;
	if (move->type == MOVE_CASTLE) return true;
	struct piece *from = get_piece(game, move->from);
	struct piece *to = get_piece(game, move->to);
	if (!from) return false;
	if (!to) return false; // cannot move to an invalid position
	if (to->type != TYPE_NONE) {
		if (to->color == player) return false;
		move->type = MOVE_CAPTURE; // set the move type to capture
	}
	if (from->type == TYPE_PAWN) {
		if (position_equal(game->en_passant_target, move->to)) {
			move->type = MOVE_CAPTURE;
			move->en_passant = true;
		}
		if (move->to.y == 0 || move->to.y == CHESS_BOARD_HEIGHT - 1) {
			// important: this code MUST be after the en passant check since en_passant and promote_to are in the same union
			if (move->type == MOVE_CAPTURE_PROMOTION || move->type == MOVE_PROMOTION) return true;
			if (move->type == MOVE_CAPTURE)
				move->type = MOVE_CAPTURE_PROMOTION;
			else
				move->type = MOVE_PROMOTION;
			// add all possible promotions
			move->promote_to = TYPE_QUEEN;
			// TODO: find out why this is causing infinite loop
			// add_move(game, list, *move);
			move->promote_to = TYPE_ROOK;
			// add_move(game, list, *move);
			move->promote_to = TYPE_BISHOP;
			// add_move(game, list, *move);
			move->promote_to = TYPE_KNIGHT;
		}
	}
	return true;
}

static bool map_legal_moves(struct game *game, struct move_list *list, enum piece_color player, void *data) {
	// does not actually remove legal moves, just sets the flag if the move is legal or not
	(void) data;
	struct move *move = &list->move;
	// check if the move puts the player in check
	struct game game_copy = *game;
	move->legal = true;
	if (!perform_move_internal(&game_copy, *move)) return false;
	move->legal = !get_if_check(&game_copy, player);
	return true;
}

static bool filter_legal_moves(struct game *game, struct move_list *list, enum piece_color player, void *data) {
	(void) player;
	(void) game;
	(void) data;
	return list->move.legal;
}

static bool annotate_moves(struct game *game, struct move_list *list, enum piece_color player, void *data) {
	struct move *move = &list->move;
	if (!move->legal) return true;

	// annotate check/stalemate/checkmate for the other player
	struct game game_copy = *game;

	if (!perform_move_internal(&game_copy, *move)) {
		fprintf(stderr, "this should not be possible\n");
		exit(1);
	}

	struct move_state state = get_move_state(&game_copy, get_opposite_color(player));
	move->state = state;

	char *notation = move->notation;
	uint8_t i = 0;

	struct piece *from = get_piece(game, move->from);
	bool is_capture = move->type == MOVE_CAPTURE || move->type == MOVE_CAPTURE_PROMOTION;
	bool is_pawn = from->type == TYPE_PAWN;

	if (move->type == MOVE_CASTLE) {
		notation[i++] = '0';
		// 0-0 for kingside, 0-0-0 for queenside
		for (uint8_t j = 0; j < (move->castle == QUEEN_SIDE ? 2 : 1); ++j) {
			notation[i++] = '-';
			notation[i++] = '0';
		}
	} else {
		// add the piece type
		if (!is_pawn) notation[i++] = piece_to_char(from->type, false);

		// check for ambiguous source file/rank
		struct move_list *whole_list = (struct move_list *) data;
		bool ambiguous_file = false, ambiguous_rank = false;
		for (struct move_list *m = whole_list; m; m = m->next) {
			if (m == list) continue; // skip the current move
			if (position_equal(m->move.to, move->to)) {
				struct piece *other = get_piece(game, m->move.from);
				if (other->type == from->type) {
					if (m->move.from.x == move->from.x) ambiguous_file = true;
					if (m->move.from.y == move->from.y) ambiguous_rank = true;
				}
			}
		}

		if (is_capture && is_pawn) ambiguous_file = true; // algebraic notation includes file if there is a capture even if it is not ambiguous

		// add the source file/rank
		if (ambiguous_file) notation[i++] = file_to_char(move->from.x);
		if (ambiguous_rank) notation[i++] = rank_to_char(move->from.y);

		// add the capture symbol
		if (is_capture) notation[i++] = 'x';

		// add the destination
		notation[i++] = file_to_char(move->to.x);
		notation[i++] = rank_to_char(move->to.y);

		// add the promotion annotation
		if (move->type == MOVE_PROMOTION || move->type == MOVE_CAPTURE_PROMOTION) {
			notation[i++] = '=';
			notation[i++] = piece_to_char(move->promote_to, false);
		}
	}

	// add the check/checkmate annotation
	if (state.check) {
		notation[i++] = state.stalemate ? '#' : '+';
	}

	// add the null terminator
	notation[i] = '\0';

	return true;
}

static struct move_list *get_available_moves_internal(struct game *game, enum piece_color player, bool check_threat) {
	struct move_list *list = alloc_move(game); // dummy node to simplify adding moves to the list
	struct position pos;
	for (pos.y = 0; pos.y < CHESS_BOARD_HEIGHT; pos.y++) {
		for (pos.x = 0; pos.x < CHESS_BOARD_WIDTH; pos.x++) {
			struct piece *piece = get_piece(game, pos);
			if (!piece) continue;
			if (piece->type == TYPE_NONE) continue;
			if (piece->color != player) continue;

			switch (piece->type) {
				case TYPE_PAWN:;
					int8_t direction = piece->color == COLOR_WHITE ? 1 : -1;
					int8_t pawn_rank = piece->color == COLOR_WHITE ? 1 : CHESS_BOARD_HEIGHT - 2;

					struct position forward = POS(pos.x, pos.y + direction);
					struct position forward2 = POS(pos.x, pos.y + 2 * direction);
					struct piece *forward_piece = get_piece(game, forward);
					struct piece *forward2_piece = get_piece(game, forward2);
					if (forward_piece && forward_piece->type == TYPE_NONE) {
						// pawn can move forward
						add_move(game, list, MOVE(pos, forward));
						if (pawn_rank == pos.y && forward2_piece && forward2_piece->type == TYPE_NONE) {
							// pawn can move forward two spaces if they haven't moved yet
							add_move(game, list, MOVE(pos, forward2));
						}
					}

					for (int8_t i = 0; i < 2; ++i) {
						int8_t offset = i * 2 - 1;
						struct position diagonal = POS(pos.x + offset, pos.y + direction);
						struct piece *diagonal_piece = get_piece(game, diagonal);
						if (!diagonal_piece) continue;
						if ((diagonal_piece->type != TYPE_NONE && diagonal_piece->color != piece->color) || (position_equal(game->en_passant_target, diagonal))) {
							// pawn can capture diagonally or en passant
							add_move(game, list, MOVE(pos, diagonal));
						}
					}
					break;
				case TYPE_KNIGHT:;
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
						add_move(game, list, MOVE(pos, POS(pos.x + directions[i].x, pos.y + directions[i].y)));
					}
					break;
				case TYPE_BISHOP:
					search_moves(game, list, pos, false, true, 0);
					break;
				case TYPE_ROOK:
					search_moves(game, list, pos, true, false, 0);
					break;
				case TYPE_QUEEN:
					search_moves(game, list, pos, true, true, 0);
					break;
				case TYPE_KING:
					search_moves(game, list, pos, true, true, 1);
					break;
				default:
					break;
			}
		}
	}
	find_castle_moves(game, list, player);
	filter_moves(game, list, player, filter_valid_moves, NULL);
	if (!check_threat) // do not bother if we are checking for check/attacks to avoid infinite recursion
		filter_moves(game, list, player, map_legal_moves, NULL);
	return list; // skip the dummy node
}

struct move_list *get_legal_moves(struct game *game) {
	struct move_list *list = get_available_moves_internal(game, game->active_color, false);
	filter_moves(game, list, game->active_color, filter_legal_moves, NULL);
	filter_moves(game, list, game->active_color, annotate_moves, (void *) list->next);
	struct move_list *new_list = list->next;
	game->free(list); // free the dummy node
	return new_list;
}

void free_move_list(struct game *game, struct move_list *list) {
	if (!list) return;
	for (; list;) {
		struct move_list *next = list->next;
		game->free(list);
		list = next;
	}
}

static bool perform_move_internal(struct game *game, struct move move) {
	if (!move.legal) return false;
	bool reset_half_move = false;
	struct position king = POS(CHESS_BOARD_WIDTH - 4, game->active_color == COLOR_WHITE ? 0 : CHESS_BOARD_HEIGHT - 1);
	struct position rook_king_side = POS(CHESS_BOARD_WIDTH - 1, king.y);
	struct position rook_queen_side = POS(0, king.y);
	if (move.type == MOVE_CASTLE) {
		int8_t direction = move.castle == KING_SIDE ? 1 : -1;
		struct position rook = move.castle == KING_SIDE ? rook_king_side : rook_queen_side;
		struct position king = POS(CHESS_BOARD_WIDTH - 4, rook.y);
		struct position new_rook = POS(king.x + direction, rook.y);
		struct position new_king = POS(king.x + direction * 2, rook.y);
		struct piece *rook_piece = get_piece(game, rook);
		struct piece *king_piece = get_piece(game, king);
		struct piece *new_rook_piece = get_piece(game, new_rook);
		struct piece *new_king_piece = get_piece(game, new_king);
		if (!rook_piece || !king_piece || !new_rook_piece || !new_king_piece) return false;

		// move the king
		*new_rook_piece = *rook_piece;
		*new_king_piece = *king_piece;
		rook_piece->type = TYPE_NONE;
		king_piece->type = TYPE_NONE;

		// disallow castling
		game->castle_availability[game->active_color] = 0;
	} else {
		struct piece *from_piece = get_piece(game, move.from);
		struct piece *to_piece = get_piece(game, move.to);
		if (!from_piece || !to_piece) return false;

		reset_half_move = from_piece->type == TYPE_PAWN || to_piece->type != TYPE_NONE; // if pawn moves or piece is captured

		// disallow castling if the king or rook moves
		// does not matter what piece is here since the
		// bits will already be unset if the pieces are not rooks or king
		if (position_equal(move.from, rook_king_side)) {
			game->castle_availability[game->active_color] &= ~GAME_CASTLE_KING_SIDE;
		} else if (position_equal(move.from, rook_queen_side)) {
			game->castle_availability[game->active_color] &= ~GAME_CASTLE_QUEEN_SIDE;
		} else if (position_equal(move.from, king)) {
			game->castle_availability[game->active_color] = 0;
		}

		*to_piece = *from_piece; // move the piece
		from_piece->type = TYPE_NONE;

		// promote the pawn
		if (move.type == MOVE_PROMOTION || move.type == MOVE_CAPTURE_PROMOTION) {
			to_piece->type = move.promote_to;
		}

		// capture the pawn en passant
		if (move.type == MOVE_CAPTURE && move.en_passant) {
			struct piece *en_passant_piece = get_piece(game, game->en_passant_target);
			if (en_passant_piece) {
				en_passant_piece->type = TYPE_NONE;
			}
		}

		// update the en passant target
		if (from_piece->type == TYPE_PAWN && abs8(move.from.y - move.to.y) == 2) {
			game->en_passant_target = POS(move.from.x, (move.from.y + move.to.y) / 2);
		} else {
			game->en_passant_target.x = 0;
			game->en_passant_target.y = 0;
		}
	}

	// update the player's turn
	game->active_color = get_opposite_color(game->active_color);

	// increment the half move counter
	if (reset_half_move)
		game->half_move = 0;
	else if (game->half_move < 50) // no need to go higher
		++game->half_move;

	// increment the move counter
	if (game->active_color == COLOR_WHITE) ++game->full_move;

	return true;
}

bool perform_move(struct game *game, struct move move) {
	bool result = perform_move_internal(game, move);
	if (!result) return false;

	// add the move to the move list
	add_move_end(game, move);
	return true;
}

enum color_opt get_winner(struct game *game) {
	switch (game->win) {
		case STATE_CHECKMATE_WHITE_WIN:
		case STATE_TIMEOUT_WHITE_WIN:
		case STATE_RESIGNATION_WHITE_WIN:
			return OPT_WHITE;
		case STATE_CHECKMATE_BLACK_WIN:
		case STATE_TIMEOUT_BLACK_WIN:
		case STATE_RESIGNATION_BLACK_WIN:
			return OPT_BLACK;
		default:
			return OPT_NONE;
	}
}

char *get_move_string(struct game *game) {
	size_t i = 0; // count number of moves to allocate the correct amount of memory
	for (struct move_list *list = game->move_list; list; list = list->next) {
		++i;
	}
	// allocate based on the max size of a move string
	char *move = game->malloc(i * (24 + sizeof(((struct move *) NULL)->notation)));
	// 24 characters to be safe, move number can theoretically be longer than 2-3 characters
	*move = '\0';

	i = 0;
	enum piece_color player = COLOR_WHITE;
	for (struct move_list *list = game->move_list; list; list = list->next) {
		if (player == COLOR_WHITE) {
			if (i > 0) strcat(move, " "); // add space between full moves
			char number[16];
			sprintf(number, "%lu.", ++i);
			strcat(move, number);
		}
		strcat(move, list->move.notation);
		strcat(move, " ");
		player = get_opposite_color(player);
	}
	if (game->win != STATE_NONE) {
		switch (get_winner(game)) {
			case OPT_WHITE:
				strcat(move, "1-0");
				break;
			case OPT_BLACK:
				strcat(move, "0-1");
				break;
			default:
				strcat(move, "1/2-1/2");
				break;
		}
	} else {
		size_t len = strlen(move);
		if (len > 0) move[len - 1] = '\0'; // remove the trailing space
	}
	return move;
}

static bool print_line(char **line, FILE *fp) {
	if (!line) return false;
	if (!*line) return false;
	size_t max_len = 0;
	for (size_t i = 1; (*line)[i] && i < 100; ++i) {
		// split at double spaces
		if ((*line)[i - 1] == ' ')
			if ((*line)[i] == ' ')
				max_len = i;
	}
	if (max_len) {
		fwrite(*line, sizeof(char), max_len, fp);
		*line = *line + max_len;
	}
	for (; **line == ' '; ++*line); // skip leading spaces
	if (!**line) {                  // end of string
		*line = NULL;
		return false;
	}
}

void print_board(struct game *game, bool unicode, bool colors, FILE *fp) {
	char *move_str = get_move_string(game);
	char *whole_move_str = move_str;
	fprintf(fp, "  ");
	for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
		fprintf(fp, "%c ", file_to_char(x));
	}
	print_line(&move_str, fp);
	fprintf(fp, "\n");
	for (uint8_t y_ = 0; y_ < CHESS_BOARD_HEIGHT; y_++) {
		uint8_t y = CHESS_BOARD_HEIGHT - 1 - y_;
		fprintf(fp, "%c ", rank_to_char(y));
		for (uint8_t x = 0; x < CHESS_BOARD_WIDTH; x++) {
			struct piece *p = get_piece(game, POS(x, y));
			char c[6];
			memset(c, 0, sizeof(c));
			if (!unicode) {
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
					c[2] = '\x93' + p->type + (p->color == COLOR_BLACK || colors ? 6 : 0);
				}
			}
			if (p->type == TYPE_NONE || !colors)
				fprintf(fp, "%s ", c);
			else if (p->color == COLOR_WHITE)
				fprintf(fp, "\033[1;47;30m%s\033[0m ", c);
			else
				fprintf(fp, "\033[1;40;37m%s\033[0m ", c);
		}
		print_line(&move_str, fp);
		fprintf(fp, "\n");
	}
	game->free(whole_move_str);
}
