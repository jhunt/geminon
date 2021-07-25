#include "./gemini.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <fcntl.h>

int gemini_handle(struct gemini_server *server, struct gemini_handler *handler) {
	handler->next = NULL;

	if (!server->first) server->first      = handler;
	if ( server->last ) server->last->next = handler;
	server->last = handler;

	return 0;
}

int gemini_handle_fn(struct gemini_server *server, const char *prefix, gemini_handler fn, void *data) {
	struct gemini_handler *handler;

	handler = malloc(sizeof(struct gemini_handler));
	if (!handler) {
		return -1;
	}

	handler->prefix  = prefix;
	handler->handler = fn;
	handler->data    = data;

	return gemini_handle(server, handler);
}

static char iocopy_buf[GEMINI_IOCOPY_BLOCK_SIZE];
static int s_iocopy(int dstfd, int srcfd) {
	ssize_t n, nread, nwrit;

	n = 0;
	while ((nread = read(srcfd, iocopy_buf+n, sizeof(iocopy_buf)-n)) > 0) {
		n += nread;
		nwrit = write(dstfd, iocopy_buf, n);
		if (nwrit < 0) return nwrit;
		memmove(iocopy_buf, iocopy_buf+nwrit, n-nwrit);
		n -= nwrit;
	}
	if (nread < 0) return nread;
	while (n > 0) {
		nwrit = write(dstfd, iocopy_buf, n);
		if (nwrit < 0) return nwrit;
		memmove(iocopy_buf, iocopy_buf+nwrit, n-nwrit);
		n -= nwrit;
	}
}

static int s_handler_fs(int connfd, struct gemini_url *url, void *_fs) {
	struct gemini_fs *fs;
	int resfd;

	fs = _fs;
	resfd = gemini_fs_open(fs, url->path, O_RDONLY);
	if (resfd < 0) {
		write(connfd, "51 Not Found\r\n", 14);
		close(connfd);
		return 0;
	}

	write(connfd, "20 text/plain\r\n", 15);
	if (s_iocopy(connfd, resfd) < 0) {
		close(resfd);
		close(connfd);
		return -1;
	}
	close(resfd);
	close(connfd);
	return 0;
}

int gemini_handle_fs(struct gemini_server *server, const char *prefix, const char *root) {
	struct gemini_fs *fs;

	fs = malloc(sizeof(struct gemini_fs));
	if (!fs) {
		return -1;
	}
	fs->root = root;

	if (gemini_handle_fn(server, prefix, s_handler_fs, fs) != 0) {
		free(fs);
		return -1;
	}

	return 0;
}

int gemini_bind(struct gemini_server *server, const char *url) {
	int fd, rc, v;
	struct sockaddr_in sa;

	server->url = gemini_parse_url("gemini://localhost:1964/");
	if (!server->url) {
		return -1;
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return fd;
	}

	v = 1;
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
	if (rc < 0) {
		close(fd);
		return rc;
	}

	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(server->url->port);
	sa.sin_addr.s_addr = INADDR_ANY;

	rc = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		close(fd);
		return rc;
	}

	rc = listen(fd, GEMINI_LISTEN_BACKLOG);
	if (rc < 0) {
		close(fd);
		return rc;
	}

	server->sockfd = fd;
	return 0;
}

static ssize_t s_readto(int fd, char *dst, size_t len, const char *end) {
	size_t i, j, ntotal = 0, nread;

	while ((nread = read(fd, dst+ntotal, len-1-ntotal)) > 0) {
		ntotal += nread;
		*(dst+ntotal) = '\0';
		if (strstr(dst, end) != NULL) {
			return ntotal;
		}
	}
	return -ntotal;
}

int gemini_serve(struct gemini_server *server) {
	int connfd;

	ssize_t n;
	char *p, buf[GEMINI_MAX_REQUEST];
	struct gemini_url *url;
	struct gemini_handler *handler;
	int handled;

	while ((connfd = accept(server->sockfd, NULL, NULL)) != -1) {
		fprintf(stderr, "[gemini_serve] accepted inbound connection on fd %d\n", connfd);

		n = s_readto(connfd, buf, sizeof(buf), "\r\n");
		if (n <= 0) {
			fprintf(stderr, "[gemini_serve] received error while reading from connection on fd %d\n", connfd);
			close(connfd);
			continue;
		}

		fprintf(stderr, "[gemini_serve] read a total of %ld bytes from connection on fd %d\n", n, connfd);

		p = strstr(buf, "\r\n");
		assert(p); *p = '\0';

		fprintf(stderr, "[gemini_serve] checking url '%s'\n", buf);
		url = gemini_parse_url(buf);
		if (!url) {
			fprintf(stderr, "[gemini_serve] '%s' is an invalid gemini:// protocol url\n", buf);
			write(connfd, "50 Bad URL\r\n", 12);
			close(connfd);
			continue;
		}

		if (strcmp(url->host, server->url->host) != 0 || url->port != server->url->port) {
			fprintf(stderr, "[gemini_serve] '%s' is for host '%s:%d' (not '%s:%d')\n",
				buf, url->host, url->port, server->url->host, server->url->port);
			write(connfd, "52 Misdirected\r\n", 16);
			close(connfd);
			continue;
		}

		/* walk the handlers */
		handled = 0;
		for (handler = server->first; handler; handler = handler->next) {
			/* FIXME check for prefix match! */
			if (handler->handler(connfd, url, handler->data) == 0) {
				handled = 1;
				break;
			}
		}
		if (!handled) {
			write(connfd, "51 Not Found\r\n", 14);
			close(connfd);
		}
	}

	return -1;
}
