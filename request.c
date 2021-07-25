#include "./gemini.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int gemini_request_respond(struct gemini_request *req, int status, const char *meta) {
	char buf[GEMINI_MAX_RESPONSE];
	memset(buf, 0, sizeof(buf));

	buf[0] = '0' + status / 10 % 10;
	buf[1] = '0' + status      % 10;
	buf[2] = ' ';

	strncpy(buf+3, meta, sizeof(buf) - 3 /*   "XX "  */
	                                 - 2 /*   \r\n   */
	                                 - 1 /*   \0     */);
	strcat(buf, "\r\n");

	return gemini_request_write(req, buf, strlen(buf));
}

int gemini_request_write(struct gemini_request *req, const void *buf, size_t n) {
	int rc;
	size_t ntotal, nwrit;

	ntotal = 0;
	while (n > 0) {
		rc = SSL_write_ex(req->ssl, buf, n, &nwrit);
		if (rc == 0) {
			return -1;
		}
		n -= nwrit;
		buf += n;
		ntotal += nwrit;
	}

	return ntotal;
}

int gemini_request_stream(struct gemini_request *req, int fd, size_t block) {
	char *buf;
	ssize_t n, nread, nwrit;

	buf = malloc(block);
	if (!buf) {
		return -1;
	}

	n = 0;
	while ((nread = read(fd, buf+n, block-n)) > 0) {
		n += nread;
		nwrit = gemini_request_write(req, buf, n);
		if (nwrit < 0) return nwrit;
		memmove(buf, buf+nwrit, n-nwrit);
		n -= nwrit;
	}
	if (nread < 0) return nread;
	while (n > 0) {
		nwrit = gemini_request_write(req, buf, n);
		if (nwrit < 0) return nwrit;
		memmove(buf, buf+nwrit, n-nwrit);
		n -= nwrit;
	}

	free(buf);
	return 0;
}

void gemini_request_close(struct gemini_request *req) {
	if (req->ssl) {
		SSL_shutdown(req->ssl);
		SSL_free(req->ssl);
		req->ssl = NULL;
	}

	if (req->fd >= 0) {
		close(req->fd);
		req->fd = -1;
	}

	if (req->url) {
		free(req->url);
		req->url = NULL;
	}
}
