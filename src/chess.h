#ifndef CHESS_H
#define CHESS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define CHESS_BOARD_WIDTH (8)
#define CHESS_BOARD_HEIGHT (8)

#define POS(x_, y_) ((struct position){.x = (x_), .y = (y_)})
#define MOVE(from_, to_) ((struct move){.from = (from_), .to = (to_), .type = MOVE_REGULAR})

struct piece {
	enum piece_type {
		TYPE_NONE = 0,
		TYPE_KING = 1,
		TYPE_QUEEN = 2,
		TYPE_ROOK = 3,
		TYPE_BISHOP = 4,
		TYPE_KNIGHT = 5,
		TYPE_PAWN = 6,
	} type;
	enum piece_color {
		COLOR_WHITE = 0,
		COLOR_BLACK = 1
	} color;
};

enum color_opt {
	OPT_WHITE = 0,
	OPT_BLACK = 1,
	OPT_NONE = 2,
};

struct position {
	int8_t x; // file
	int8_t y; // rank
};

#define GAME_CASTLE_KING_SIDE (1 << 0)
#define GAME_CASTLE_QUEEN_SIDE (1 << 1)
#define GAME_CASTLE_ALL (GAME_CASTLE_KING_SIDE | GAME_CASTLE_QUEEN_SIDE)

struct move {
	bool legal;
	enum move_type {
		MOVE_REGULAR,
		MOVE_CAPTURE,
		MOVE_CASTLE,
		MOVE_PROMOTION,
		MOVE_CAPTURE_PROMOTION
	} type;
	union {
		struct {
			struct {
				struct position from;
				struct position to;
			};
			union {
				bool en_passant;
				enum piece_type promote_to;
			};
		};
		enum castle_side {
			KING_SIDE,
			QUEEN_SIDE
		} castle;
	};
	char notation[16];
	struct move_state {
		bool stalemate, check;
	} state;
};

struct game {
	void *(*malloc)(size_t);
	void (*free)(void *);
	struct piece board[CHESS_BOARD_HEIGHT][CHESS_BOARD_WIDTH];
	enum piece_color active_color;
	uint8_t castle_availability[2];
	struct position en_passant_target;

	uint8_t half_move;
	size_t full_move;

	struct move_list {
		struct move move;
		struct move_list *next;
	} *move_list, *move_list_tail;

	enum win_state {
		STATE_NONE,

		// TODO: detect checkmate
		STATE_CHECKMATE_WHITE_WIN,
		STATE_CHECKMATE_BLACK_WIN,
		// TODO: detect timeout and handle resignation
		STATE_TIMEOUT_WHITE_WIN,
		STATE_TIMEOUT_BLACK_WIN,
		STATE_RESIGNATION_WHITE_WIN,
		STATE_RESIGNATION_BLACK_WIN,

		// TODO: detect stalemate
		STATE_STALEMATE,
		// TODO: detect insufficient material
		STATE_INSUFFICIENT_MATERIAL,         // both players have insufficient material
		STATE_TIMEOUT_INSUFFICIENT_MATERIAL, // player 1 has insufficient material and player 2 runs out of time

		// TODO: handle logic for other draw conditions

		STATE_FIFTY_MOVE_RULE,
		STATE_THREEFOLD_REPETITION,
		STATE_AGREED_DRAW,
	} win;
};

char piece_to_char(enum piece_type type, bool lowercase);
char piece_to_char_struct(struct piece piece);
struct piece char_to_piece(char);
char rank_to_char(uint8_t rank);
char file_to_char(uint8_t file);
int8_t char_to_rank(char str);
int8_t char_to_file(char str);

enum piece_color get_opposite_color(enum piece_color color);

bool position_equal(struct position a, struct position b);
bool position_valid_xy(int8_t x, int8_t y);
bool position_valid(struct position pos);
struct piece *get_piece_xy(struct game *game, int8_t x, int8_t y);
struct piece *get_piece(struct game *game, struct position pos);

struct move_list *add_move(struct game *game, struct move_list *list, struct move move);

enum find_move_reason {
	REASON_SUCCESS, REASON_WIN, REASON_AMBIGUOUS, REASON_ILLEGAL, REASON_SYNTAX, REASON_NONE_FOUND
};

struct move_list *get_legal_moves(struct game *game);
enum find_move_reason find_move(struct game *game, struct move *out_move, const char *input);
void free_move_list(struct game *game, struct move_list *list);
bool perform_move(struct game *game, struct move move);

enum color_opt get_winner(struct game *game);
struct game *create_board(void *(*malloc_)(size_t), void (*free_)(void *));
void destroy_board(struct game *game);
void board_init(struct game *game);
char *get_move_string(struct game *game);
#endif
