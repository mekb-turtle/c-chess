#include <stdbool.h>
#include <stddef.h>
#include <sys/socket.h>

bool parse_address(const char *str, char *err, size_t err_n, struct sockaddr *addr);
int create_socket(struct sockaddr *addr);
