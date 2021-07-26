#include "./gemini.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

ssize_t gemini_response_read(struct gemini_response *res, void *buf, size_t n) {
	int rc;
	size_t ntotal;
	int nread;

	ntotal = 0;
	while (n > 0 && (nread = SSL_read(res->ssl, buf, n)) > 0) {
		n -= nread;
		buf += nread;
		ntotal += nread;
	}
	if (nread < 0) {
		return nread;
	}

	return ntotal;
}

int gemini_response_stream(struct gemini_response *res, int fd, size_t block) {
	char *buf;
	ssize_t n, nread, nwrit;

	buf = malloc(block);
	if (!buf) {
		return -1;
	}

	n = 0;
	while ((nread = gemini_response_read(res, buf+n, block-n)) > 0) {
		n += nread;
		nwrit = write(fd, buf, n);
		if (nwrit < 0) return nwrit;
		memmove(buf, buf+nwrit, n-nwrit);
		n -= nwrit;
	}
	if (nread < 0) return nread;
	while (n > 0) {
		nwrit = write(fd, buf, n);
		fprintf(stderr, "wrote %ldb to stream\n", nwrit);
		if (nwrit < 0) return nwrit;
		memmove(buf, buf+nwrit, n-nwrit);
		n -= nwrit;
	}

	free(buf);
	return 0;
}

void gemini_response_close(struct gemini_response *res) {
	if (res->ssl) {
		SSL_shutdown(res->ssl);
		SSL_free(res->ssl);
		res->ssl = NULL;
	}

	if (res->fd >= 0) {
		close(res->fd);
		res->fd = -1;
	}

	if (res->url) {
		free(res->url);
		res->url = NULL;
	}
}
