#include "./gemini.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <fcntl.h>

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

int gemini_serve(struct gemini_server *server) {
	int connfd, resfd;

	ssize_t n;
	char *p, buf[GEMINI_MAX_REQUEST];
	struct gemini_url *url;
	struct gemini_fs fs;

	fs.root = server->root;
	if (fs.root == NULL || !*fs.root) {
		return -1;
	}

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

		fprintf(stderr, "[gemini_serve] fetching '%s'\n", url->path);
		resfd = gemini_fs_open(&fs, url->path, O_RDONLY);
		if (resfd < 0) {
			fprintf(stderr, "[gemini_serve] '%s': not found\n", url->path);
			write(connfd, "51 Not Found\r\n", 14);
			close(connfd);
			continue;
		}

		write(connfd, "20 text/plain\r\n", 15);
		if (s_iocopy(connfd, resfd) < 0) {
			fprintf(stderr, "[gemini_serve] '%s': failed copying to connected client\n", url->path);
		}
		close(resfd);

		fprintf(stderr, "[gemini_serve] closing connection on fd %d\n", connfd);
		close(connfd);
	}
}
