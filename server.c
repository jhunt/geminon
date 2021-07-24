#include "./gemini.h"

#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

int gemini_bind(struct gemini_server *server) {
	int fd, rc;
	struct sockaddr_in sa;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return fd;
	}

	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(GEMINI_DEFAULT_PORT);
	sa.sin_addr.s_addr = INADDR_ANY;

	rc = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		close(fd);
		return rc;
	}

	rc = listen(fd, GEMINI_FIXME_LISTEN_BACKLOG);
	if (rc < 0) {
		close(fd);
		return rc;
	}

	server->sockfd = fd;
	return 0;
}

int gemini_serve(struct gemini_server *server) {
	int connfd;

	size_t ntotal, nread;
	char buf[MAX_GEMINI_REQUEST_SIZE];

	while ((connfd = accept(server->sockfd, NULL, NULL)) != -1) {
		fprintf(stderr, "[gemini_serve] accepted inbound connection on fd %d\n", connfd);

		while ((nread = read(connfd, buf+ntotal, MAX_GEMINI_REQUEST_SIZE - ntotal)) > 0) {
			fprintf(stderr, "[gemini_serve] read %ld bytes from connection on fd %d\n", nread, connfd);
			ntotal += nread;
		}
		if (nread != 0) {
			fprintf(stderr, "[gemini_serve] received error while reading from connection on fd %d\n", connfd);
			close(connfd);
			continue;
		}

		fprintf(stderr, "[gemini_serve] read a total of %ld bytes from connection on fd %d\n", ntotal, connfd);
		fprintf(stderr, "[gemini_serve] read [%.*s]\n", (int)ntotal, buf);

		fprintf(stderr, "[gemini_serve] closing connection on fd %d\n", connfd);
		close(connfd);
	}
}
