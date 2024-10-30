#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "socket.h"
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

bool parse_address(const char *str, char *err, size_t err_n, struct sockaddr *addr) {
	if (!str) return false;

	struct sockaddr_un un;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;

	if (strchr(str, ':')) { // IPv4 or IPv6
		char *p = strrchr(str, ':');
		if (!p || !valid_number(p + 1, 5)) {
		invalid_port:
			if (err) snprintf(err, err_n, "Invalid port");
			return false;
		}
		int port = atoi(p + 1);
		if (port <= 0 || port > 65535) goto invalid_port;

		memset(&in, 0, sizeof(in));
		memset(&in6, 0, sizeof(in6));

		size_t len = p - str;
		char addr_str[len + 1];
		strncpy(addr_str, str, len);
		addr_str[len] = '\0';

		if (inet_pton(AF_INET6, addr_str, &in6.sin6_addr)) {
			in6.sin6_family = AF_INET6;
			in6.sin6_port = htons(port);
			*(struct sockaddr_in6 *) addr = in6;
			return true;
		} else if (inet_pton(AF_INET, addr_str, &in.sin_addr)) {
			in.sin_family = AF_INET;
			in.sin_port = htons(port);
			*(struct sockaddr_in *) addr = in;
			return true;
		} else {
			if (err) snprintf(err, err_n, "Invalid address");
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
		strncpy(un.sun_path, str, sizeof(un.sun_path) - 1);
		*(struct sockaddr_un *) addr = un;
		return true;
	}
}

int create_socket(struct sockaddr *addr) {
	return socket(addr->sa_family, SOCK_STREAM, 0);
}
