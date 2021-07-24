#include "./fsm.c"
#include "./gemini.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

struct gemini_url * gemini_new_url(unsigned int len) {
	struct gemini_url *url;

	url = malloc(sizeof(struct gemini_url) + len);
	if (!url) {
		return NULL;
	}
	url->len = len;

	return url;
}

struct gemini_url * gemini_parse_url(const char *s) {
	struct gemini_url *url;

	url = gemini_new_url(strlen(s) + 1);
	if (!url) {
		return NULL;
	}

	if (gemini_parse_url_into(s, url) != 0) {
		return url;
	}

	free(url);
	return NULL;
}

int gemini_parse_url_into(const char *s, struct gemini_url *url) {
	int state, to, port = GEMINI_DEFAULT_PORT;
	const char *next;
	char *fill;

	if (url == NULL) {
		return -91;
	}

	for (state = 0, fill = url->buf, next = s; *next; next++) {
		if (fill >= url->buf + url->len) {
			return -92;
		}

		to = STATES[state][*next & 0xff];
		if (to < 0) {
			return -93;
		}

		switch (state * 100 + to) {
		case 910: /* 9 -> 10 = start of host */
			url->host = fill;
			*fill++ = *next;
			break;

		case 1010: /* 10 -> 10 = more host characters; continue fill */
			*fill++ = *next;
			break;

		case 1012: /* 10 -> 12 = end of host (/path variant) */
			*fill++ = '\0';
			url->path = fill;
			*fill++ = *next;
			break;

		case 1011: /* 10 -> 11 = end of host (:port variant) */
			*fill++ = '\0';
			port = 0;
			break;

		case 1112: /* 11 -> 12 = end of port, start of path */
			url->path = fill;
		case 1212: /* 12 -> 12 = continuation of path */
			*fill++ = *next;
			break;

		case 1111: /* 11 -> 11 = another port digit */
			port = port * 10 + (*next - '0');
			if (port > 0xffffu) {
				return -95;
			}
			break;
		}

		state = to;
	}

	url->port = port & 0xffffu;
	if (state == 12) {
		*fill = '\0';
		return 0;

	} else {
		return -100 - state;
	}
}
