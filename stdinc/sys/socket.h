#pragma once

#include "compiler.h"
#include <uk/socket.h>

BEGIN_DECLS

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int socket(int domain, int type, int protocol);

END_DECLS
