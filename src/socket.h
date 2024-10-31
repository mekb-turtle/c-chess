#ifndef SOCKET_H
#define SOCKET_H
#include <stdbool.h>
#include <stddef.h>
#include <sys/socket.h>
#include "chess.h"

size_t get_sockaddr_len();
bool parse_address(const char *str, char *err, size_t err_n, struct sockaddr *out_addr, size_t *out_size, char **out_file);
bool create_server_socket(const char *str, char *err, size_t err_n, int *out_sockfd, char **out_file);
bool create_client_socket(const char *str, char *err, size_t err_n, int *out_sockfd, char **out_file);

enum socket_signal_type {
	SIGNAL_GAME_DATA = 1,
	SIGNAL_CLEAR_MOVES = 2,
	SIGNAL_ADD_MOVE = 3,
	SIGNAL_IS_TURN = 4,
};
struct socket_signal {
	uint8_t type; // enum socket_signal_type
	union {
		struct game game;
		struct move move;
		bool is_turn;
	} data;
};

ssize_t send_game(struct game *game, int fd);
ssize_t send_move(struct game *game, int fd);
ssize_t send_turn(struct game *game, int fd, enum piece_color player1_color);
bool read_signal(struct game *game, bool *is_turn, int fd);
#endif
