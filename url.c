#include "./fsm.c"
#include "./gemini.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

struct gemini_url * gemini_parse_url(const char *s) {
	int rc;
	struct gemini_url *url;

	url = malloc(sizeof(struct gemini_url) + strlen(s) + 1);
	if (!url) {
		return NULL;
	}

	rc = gemini_parse_url_into(s, url);
	if (rc == 0) {
		return url;
	}

	free(url);
	return NULL;
}

int gemini_parse_url_into(const char *s, struct gemini_url *url) {
	int state, to, port = DEFAULT_GEMINI_PORT;
	unsigned int n;
	const char *next;

	if (url == NULL) {
		return -91;
	}

	for (n = state = 0, next = s; *next; next++) {
		if (n > url->len) {
			return -92;
		}

		url->buf[n] = *next;

		to = STATES[state][*next & 0xff];
		if (to < 0) {
			return -93;
		}

		switch (state * 100 + to) {
		case 910: /* 9 -> 10 = start of host */
			url->host = url->buf + n;
			break;

		case 1012: /* 10 -> 12 = end of host (/path variant) */
			url->buf[n] = '\0';
			url->path = url->buf+n+1;
			break;

		case 1011: /* 10 -> 11 = end of host (:port variant) */
			url->buf[n] = '\0';
			port        = 0;
			break;

		case 1112: /* 11 -> 12 = end of port, start of path */
			url->buf[n] = '\0';
			url->path = url->buf+n+1;
			break;

		case 1111: /* 11 -> 11 = another port digit */
			port = port * 10 + (*next - '0');
			if (port > 0xffffu) {
				return -95;
			}
			break;
		}

		n++;
		state = to;
	}

	url->port = port & 0xffffu;
	return state == 12 ? 0 : -state;
}

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	int i, rc;
	struct gemini_url *url;

	url = malloc(sizeof(struct gemini_url) + 1024);
	if (!url) {
		fprintf(stderr, "malloc failed\n");
		return 1;
	}
	url->len = 1024;

	for (i = 1; i < argc; i++) {
		fprintf(stdout, "in: %s\n", argv[i]);
		rc = gemini_parse_url_into(argv[i], url);
		fprintf(stdout, "  = %d\n", rc);

		if (rc == 0) {
			fprintf(stdout, "  > host: %s\n", url->host);
			fprintf(stdout, "  > port: %d\n", url->port);
			fprintf(stdout, "  > path: %s\n", url->path);
		}
	}
}
