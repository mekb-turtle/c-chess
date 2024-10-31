#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "socket.h"
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

size_t get_sockaddr_len() {
	// get max size to allocate sockaddr
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c) MAX(MAX(a, b), c)
	return MAX3(sizeof(struct sockaddr_un), sizeof(struct sockaddr_in), sizeof(struct sockaddr_in6));
}

static bool valid_number(const char *str, size_t max_len) {
	if (!str) return false;
	if (!*str) return false; // empty port
	bool zero = false;
	for (const char *c = str; *c; ++c) {                    // check if port is digits with length [1,max_len]
		if ((size_t) (c - str) > max_len + 1) return false; // too long
		if (*c < '0' || *c > '9') return false;
		if (*c == '0' && !zero) return false; // no leading zeros
		zero = true;
	}
	return true;
}

bool parse_address(const char *str, char *err, size_t err_n, struct sockaddr *out_addr, size_t *out_size, char **out_file) {
	if (!str) {
		if (err) snprintf(err, err_n, "No path specified");
		return false;
	}

	struct sockaddr_un un;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;

	if (str[0] != '/' && strchr(str, ':')) { // IPv4 or IPv6
		char *p = strrchr(str, ':');
		if (!p || !valid_number(p + 1, 5)) {
		invalid_port:
			if (err) snprintf(err, err_n, "Invalid port");
			return false;
		}
		int port = atoi(p + 1);
		if (port <= 0 || port > 0xffff) goto invalid_port;

		memset(&in, 0, sizeof(in));
		memset(&in6, 0, sizeof(in6));

		size_t len = p - str;
		char addr_str[len + 1];
		strncpy(addr_str, str, len);
		addr_str[len] = '\0';

		if (inet_pton(AF_INET6, addr_str, &in6.sin6_addr)) {
			in6.sin6_family = AF_INET6;
			in6.sin6_port = htons(port);
			*(struct sockaddr_in6 *) out_addr = in6;
			*out_size = sizeof(in6);
			return true;
		} else if (inet_pton(AF_INET, addr_str, &in.sin_addr)) {
			in.sin_family = AF_INET;
			in.sin_port = htons(port);
			*(struct sockaddr_in *) out_addr = in;
			*out_size = sizeof(in);
			return true;
		} else {
			if (err) snprintf(err, err_n, "Invalid IP address");
			return false;
		}

		return true;
	} else { // unix socket
		memset(&un, 0, sizeof(un));
		un.sun_family = AF_UNIX;

		if (strlen(str) >= sizeof(un.sun_path)) {
			if (err) snprintf(err, err_n, "Path too long");
			return false;
		}
		if (out_file) *out_file = str;
		strncpy(un.sun_path, str, sizeof(un.sun_path) - 1);
		*(struct sockaddr_un *) out_addr = un;
		*out_size = sizeof(un);
		return true;
	}
}

bool create_server_socket(const char *str, char *err, size_t err_n, int *out_sockfd, char **out_file) {
	if (!out_sockfd) {
		if (err) snprintf(err, err_n, "Invalid arguments");
		return false;
	}

	const size_t sockaddr_len = get_sockaddr_len();
	size_t sockaddr_size;

	uint8_t sockaddr_[sockaddr_len];
	struct sockaddr *sockaddr = (struct sockaddr *) sockaddr_;
	char err_[err_n];
	if (!parse_address(str, err_, sizeof(err_), sockaddr, &sockaddr_size, out_file)) {
		if (err) snprintf(err, err_n, "Failed to parse server socket: %s", err_);
		return false;
	}

	int sockfd;
	if ((sockfd = socket(sockaddr->sa_family, SOCK_STREAM, 0)) == -1) {
		if (err) snprintf(err, err_n, "Failed to create server socket: %s", strerror(errno));
		return false;
	}
	if (bind(sockfd, sockaddr, sockaddr_size) < 0) {
		if (err) snprintf(err, err_n, "Failed to bind server socket: %s", strerror(errno));
		close(sockfd);
		return false;
	}
	if (listen(sockfd, 1) < 0) {
		if (err) snprintf(err, err_n, "Failed to listen on server socket: %s", strerror(errno));
		close(sockfd);
		return false;
	}
	*out_sockfd = sockfd;
	return true;
}

bool create_client_socket(const char *str, char *err, size_t err_n, int *out_sockfd, char **out_file) {
	if (!out_sockfd) {
		if (err) snprintf(err, err_n, "Invalid arguments");
		return false;
	}

	const size_t sockaddr_len = get_sockaddr_len();
	size_t sockaddr_size;

	uint8_t sockaddr_[sockaddr_len];
	struct sockaddr *sockaddr = (struct sockaddr *) sockaddr_;
	char err_[err_n / sizeof(char)];
	if (!parse_address(str, err_, sizeof(err_), sockaddr, &sockaddr_size, out_file)) {
		if (err) snprintf(err, err_n, "Failed to parse server socket: %s", err_);
		return false;
	}

	int sockfd;
	if ((sockfd = socket(sockaddr->sa_family, SOCK_STREAM, 0)) == -1) {
		if (err) snprintf(err, err_n, "Failed to create client socket: %s", strerror(errno));
		return false;
	}
	if (connect(sockfd, sockaddr, sockaddr_size) < 0) {
		if (err) snprintf(err, err_n, "Failed to connect to socket: %s", strerror(errno));
		close(sockfd);
		return false;
	}
	*out_sockfd = sockfd;
	return true;
}

ssize_t send_game(struct game *game, int fd) {
	// send game data
	struct game buf = export_board(game);
	struct socket_signal signal = {
	        .type = SIGNAL_GAME_DATA,
	        .data = {
	                .game = buf}};
	return write(fd, &signal, sizeof(signal));
}

ssize_t send_move(struct game *game, int fd) {
	// send last move
	struct move_list *move = game->move_list_tail;
	struct socket_signal signal = {
	        .type = SIGNAL_ADD_MOVE};
	if (!move) {
		// clear if no moves
		signal.type = SIGNAL_CLEAR_MOVES;
	} else {
		signal.data.move = move->move;
	}
	return write(fd, &signal, sizeof(signal));
}

ssize_t send_turn(struct game *game, int fd, enum piece_color player1_color) {
	// send if it's the player's turn
	struct socket_signal signal = {
	        .type = SIGNAL_IS_TURN,
	        .data = {
	                .is_turn = game->active_color == player1_color}};
	return write(fd, &signal, sizeof(signal));
}

bool read_signal(struct game *game, bool *is_turn, int fd) {
	struct socket_signal signal;
	ssize_t n = read(fd, &signal, sizeof(signal));
	if (n != sizeof(signal)) {
		perror("Failed to read signal");
		return false;
	}

	switch (signal.type) {
		case SIGNAL_GAME_DATA:
			import_board(game, signal.data.game);
			break;
		case SIGNAL_ADD_MOVE:
			add_move_list_end(game, signal.data.move);
			break;
		case SIGNAL_CLEAR_MOVES:
			free_move_list(game, game->move_list);
			break;
		case SIGNAL_IS_TURN:
			*is_turn = signal.data.is_turn;
			break;
	}
	return true;
}
