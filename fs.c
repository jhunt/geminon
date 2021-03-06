#include "./gemini.h"
#include "./fsm.fs.c"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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
	size_t left;
	char *fill;

	left = sizeof(p->buf) - 1;
	for (state = 1, fill = p->buf; *p->src; p->src++) {
		to = STATES[state][*p->src & 0xff];
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
			if (left == 0) {
				/* oops.  path component to long for buffer */
				return PARSED_ERR;
			}
			*fill++ = *p->src;
			left--;
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

char * gemini_fs_resolve(const char *file) {
	char *path, *p, *q;
	int deep = 0;
	size_t left;
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
			left = GEMINI_MAX_PATH - strlen(path) - 1;

			for (p = path; *p; p++) ;
			if (deep > 0) {
				/* append a slash and the directory component */
				*p++ = '/';
				left--;
			}
			for (q = parser.buf; left; *p++ = *q++, left--)
				;
			*p = '\0';
			deep++;
			break;

		case PARSED_UP:
			if (deep > 0) {
				p = strrchr(path, '/');
				if (!p) p = path;
				*p = '\0';
				deep--;
			}
			break;

		case PARSED_END:
			return path;
		}
	}
}

int gemini_fs_open(struct gemini_fs *fs, const char *file, int flags) {
	char *path;
	int dirfd, fd, rc;
	struct stat st;

	dirfd = open(fs->root, O_RDONLY);
	if (dirfd < 0) {
		return -1;
	}

	path = gemini_fs_resolve(file);
	if (!path) {
		return -1;
	}

	fd = openat(dirfd, path, flags);
	free(path);
	if (fd < 0) {
		return -1;
	}

	rc = fstat(fd, &st);
	if (rc != 0) {
		close(fd);
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return -1;
	}

	return fd;
}

char * gemini_fs_path(struct gemini_fs *fs, const char *file) {
	char *path, *resolved;
	int l1, l2;

	resolved = gemini_fs_resolve(file);
	if (!resolved) {
		return NULL;
	}

	l1 = strlen(fs->root);
	l2 = strlen(resolved);

	path = malloc(l1 + 1 + l2 + 1);
	if (!path) {
		free(resolved);
		return NULL;
	}

	memcpy(path, fs->root, l1);

	while (l1 > 0 && *(path + l1 - 1) == '/') l1--;
	*(path+l1) = '/'; l1++;

	memcpy(path+l1, resolved, l2);
	*(path+l1+l2) = '\0';

	free(resolved);
	return path;
}
