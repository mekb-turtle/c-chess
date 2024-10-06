#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define CHESS_BOARD_WIDTH (8)
#define CHESS_BOARD_HEIGHT (8)

struct piece {
	enum type {
		NONE = 0,
		PAWN = 1,
		KNIGHT = 2,
		BISHOP = 3,
		ROOK = 4,
		QUEEN = 5,
		KING = 6
	} type;
	enum color {
		WHITE = 0,
		BLACK = 1
	} color;
};

struct position {
	int8_t x;
	int8_t y;
};

struct game {
	void *(*malloc)(size_t);
	void (*free)(void *);
	struct piece board[CHESS_BOARD_HEIGHT][CHESS_BOARD_WIDTH];
	enum color active_color;
	struct castle_flags {
		struct castle_piece {
			bool king, queen;
		} white, black;
	} castle;
	struct position en_passant;
	uint8_t halfmove;
	uint16_t fullmove;
};

struct move {
	struct position from;
	struct position to;
	bool capture, en_passant;
	struct castle_piece castle;
	struct move_promotion {
		bool promotion;
		enum type type;
	} promotion;
	struct move_state {
		bool stalemate, check;
	} state;
};

char piece_to_letter(struct piece piece);
struct piece letter_to_piece(char letter);
char rank_to_letter(uint8_t rank);
char file_to_letter(uint8_t file);

struct piece *get_piece(struct game *game, struct position pos);
bool position_equal(struct position a, struct position b);
bool position_valid(struct position pos);

struct move_list {
	struct move move;
	struct move_list *next;
};
void add_move(struct game *game, struct move_list *list, struct move move);

bool loop_pieces_between(struct game *game, struct position from, struct position to, bool (*callback)(struct position, void *), void *data);
bool loop_board(struct game *game, bool (*callback)(struct position, void *), void *data);
struct move_state get_move_state(struct game *game, enum color player);

struct move_list *get_legal_moves(struct game *game, struct position pos);

void chess_init(struct game *game);
void chess_print(struct game *game);
