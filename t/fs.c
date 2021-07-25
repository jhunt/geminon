#include "./ctap.h"
#include "../gemini.h"

#define VALID     1
#define NOT_VALID 0

struct test {
	const char *name;
	const char *in;
	int         valid;
	const char *out;
};

static inline void run_resolve_tests() {
	char *path;
	int i;
	struct test cases[] = {
		{
			.name  = "absolute path",
			.in    = "/foo/bar",
			.valid = VALID,
			.out   = "foo/bar",
		},
		{
			.name  = "relative path",
			.in    = "bar/baz",
			.valid = VALID,
			.out   = "bar/baz",
		},
		{
			.name  = "dotted up directory",
			.in    = "/foo/bar/../baz",
			.valid = VALID,
			.out   = "foo/baz",
		},
		{
			.name  = "escape attempt",
			.in    = "/../../../../../../../../etc/shadow",
			.valid = VALID,
			.out   = "etc/shadow",
		},
		{
			.name  = "escape attempt",
			.in    = "/..",
			.valid = VALID,
			.out   = "",
		},
		{
			.name  = "escape attempt",
			.in    = "/../.",
			.valid = VALID,
			.out   = "",
		},
		{
			.name  = "hidden files",
			.in    = "/.hidden/file",
			.valid = VALID,
			.out   = ".hidden/file",
		},
		{
			.name  = "hidden files",
			.in    = "/.hidden/..file",
			.valid = VALID,
			.out   = ".hidden/..file",
		},
		{
			.name  = "dotted directories",
			.in    = "/something.d/test",
			.valid = VALID,
			.out   = "something.d/test",
		},
	};

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (cases[i].valid == NOT_VALID) {
			/* invalid case */
			continue;
		}

		/* valid case */
		path = gemini_fs_resolve(cases[i].in);
		ok(path, "%s '%s' should resolve properly", cases[i].name, cases[i].in);
		is(path, cases[i].out,
			"%s '%s' should resolve to '%s'", cases[i].name, cases[i].in, cases[i].out);
		free(path);
	}
}

static inline void run_open_tests() {
	int fd;
	struct gemini_fs fs;
	char buf[1024];
	size_t n;

	fs.root = "t/data";
	fd = gemini_fs_open(&fs, "non/existent/path", O_RDONLY);
	cmp_ok(fd, "<", 0, "should be unable to open [t/data/]non/existent/path");

	fd = gemini_fs_open(&fs, "foo", O_RDONLY);
	cmp_ok(fd, "<", 0, "should be unable to open [t/data/]foo (it's a directory!)");

	fd = gemini_fs_open(&fs, "foo/bar/baz/quux", O_RDONLY);
	cmp_ok(fd, ">=", 0, "should be able to open [t/data/]foo/bar/baz/quux for reading");
	n = read(fd, buf, sizeof(buf) - 1);
	cmp_ok(n, ">", 0, "should be able to read at least one byte from [t/data/]foo/bar/baz/quux");
	if (n < 0) return;
	buf[n] = '\0';
	is(buf, "a test file\n", "should be able to read [t/data/]foo/bar/baz/quux via the returned file descriptor");
}

TESTS {
	run_resolve_tests();
	run_open_tests();
}
