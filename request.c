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
	size_t nwrit;

	while ((nwrit = write(req->fd, buf, n)) > 0) {
		n -= nwrit;
		buf += n;
	}
	return nwrit;
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
	if (req->fd >= 0) {
		close(req->fd);
		req->fd = -1;
	}

	if (req->url) {
		free(req->url);
		req->url = NULL;
	}
}
