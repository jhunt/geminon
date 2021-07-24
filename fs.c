#include "./gemini.h"
#include "./fsm.fs.c"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct _parser {
	const char *src;           /* what's left to be parsed */
	char buf[GEMINI_MAX_PATH]; /* component built so far */
};

#define FROM(a,b) ((a) * 100 + (b))

#define PARSED_ERR  -1
#define PARSED_DIR   0
#define PARSED_UP    1
#define PARSED_END   2

static int s_parse_path(struct _parser *p) {
	int state, to;
	char *fill;

	for (state = 1, fill = p->buf; *p->src; p->src++) {
		to = STATES[state][*p->src];
		if (to < 0) {
			return PARSED_ERR;
		}

		switch (FROM(state,to)) {
		case FROM(3,1): /* 3 -> 1 = ".."; up a directory */
			return PARSED_UP;

		case FROM(4,1): /* 4 -> 1 = found a component */
			*fill = '\0';
			return PARSED_DIR;

		case FROM(1,2):
		case FROM(1,4):
		case FROM(2,3):
		case FROM(2,4):
		case FROM(3,4):
		case FROM(4,4):
			*fill++ = *p->src;
			// FIXME check bounds on fill access
			break;
		}

		state = to;
	}

	switch (state) {
	case 3:                return PARSED_UP;
	case 4:  *fill = '\0'; return PARSED_DIR;
	default:               return PARSED_END;
	}
}

char * gemini_fs_resolve(struct gemini_fs *fs, const char *file) {
	char *path, *p;
	int deep = 0, parsed;
	struct _parser parser;

	if (file == NULL) {
		return NULL;
	}

	memset(&parser, 0, sizeof(parser));
	parser.src = file;

	path = malloc(GEMINI_MAX_PATH+1);
	if (!path) {
		return NULL;
	}
	memset(path, 0, GEMINI_MAX_PATH+1);

	for (;;) {
		switch (s_parse_path(&parser)) {
		case PARSED_ERR:
			free(path);
			return NULL;

		case PARSED_DIR:
			/* append a slash and the directory component */
			/* FIXME replace strncats with pointer math */
			strncat(path, "/", 1);
			strncat(path, parser.buf, GEMINI_MAX_PATH - strlen(path));
			deep++;
			break;

		case PARSED_UP:
			if (deep > 0) {
				p = strrchr(path, '/');
				assert(p); *p = '\0';
				deep--;
			}
			break;

		case PARSED_END:
			return path;
		}
	}
}
