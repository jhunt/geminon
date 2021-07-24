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

TESTS {
	char *path;
	int i;
	struct test cases[] = {
		{
			.name  = "absolute path",
			.in    = "/foo/bar",
			.valid = VALID,
			.out   = "/foo/bar",
		},
		{
			.name  = "relative path",
			.in    = "bar/baz",
			.valid = VALID,
			.out   = "/bar/baz",
		},
		{
			.name  = "dotted up directory",
			.in    = "/foo/bar/../baz",
			.valid = VALID,
			.out   = "/foo/baz",
		},
		{
			.name  = "escape attempt",
			.in    = "/../../../../../../../../etc/shadow",
			.valid = VALID,
			.out   = "/etc/shadow",
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
			.out   = "/.hidden/file",
		},
		{
			.name  = "hidden files",
			.in    = "/.hidden/..file",
			.valid = VALID,
			.out   = "/.hidden/..file",
		},
		{
			.name  = "dotted directories",
			.in    = "/something.d/test",
			.valid = VALID,
			.out   = "/something.d/test",
		},
	};

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (cases[i].valid == NOT_VALID) {
			/* invalid case */
			continue;
		}

		/* valid case */
		path = gemini_fs_resolve(NULL, cases[i].in);
		ok(path, "%s '%s' should resolve properly", cases[i].name, cases[i].in);
		is(path, cases[i].out,
			"%s '%s' should resolve to '%s'", cases[i].name, cases[i].in, cases[i].out);
		free(path);
	}
}
