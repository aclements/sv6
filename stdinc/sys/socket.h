#pragma once

#include "compiler.h"
#include <uk/socket.h>

BEGIN_DECLS

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
ssize_t send(int sockfd, const void *msg, size_t len, int flags);
ssize_t sendto(int sockfd, const void *msg, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
int socket(int domain, int type, int protocol);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);

END_DECLS
