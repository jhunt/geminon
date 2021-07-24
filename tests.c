#include "./ctap.h"

#include "./gemini.h"

#define VALID     1
#define NOT_VALID 0

struct test {
	const char *name;
	const char *url;
	int         valid;

	const char     *host;
	unsigned short  port;
	const char     *path;
};

TESTS {
	struct gemini_url *u;
	int i;
	struct test cases[] = {
		{
			.name = "the empty string",
			.url  = "",
			.valid = NOT_VALID,
		},
		{
			.name  = "an HTTP URL",
			.url   = "http://huntprod.com",
			.valid = NOT_VALID,
		},
		{
			.name  = "missing path",
			.url   = "gemini://just.a.host",
			.valid = NOT_VALID,
		},
		{
			.name  = "host with invalid characters",
			.url   = "gemini://host!/",
			.valid = NOT_VALID,
		},
		{
			.name  = "host with invalid characters",
			.url   = "gemini://--the-host--/",
			.valid = NOT_VALID,
		},
		{
			.name  = "non-numeric port",
			.url   = "gemini://host:gemini/",
			.valid = NOT_VALID,
		},
		{
			.name  = "non-numeric port",
			.url   = "gemini://host:19sixty5/",
			.valid = NOT_VALID,
		},

		{
			.name  = "base case",
			.url   = "gemini://x/",
			.valid = VALID,
			.host  = "x",
			.port  = 0,
			.path  = "/",
		},
		{
			.name  = "dotted domain",
			.url   = "gemini://huntprod.com/",
			.valid = VALID,
			.host  = "huntprod.com",
			.port  = 0,
			.path  = "/",
		},
		{
			.name  = "ipv4 address",
			.url   = "gemini://192.168.88.100/",
			.valid = VALID,
			.host  = "192.168.88.100",
			.port  = 0,
			.path  = "/",
		},
		{
			.name  = "explicit port",
			.url   = "gemini://host:2021/",
			.valid = VALID,
			.host  = "host",
			.port  = 2021,
			.path  = "/",
		},
		{
			.name  = "longer path",
			.url   = "gemini://host/a/path/for/testing",
			.valid = VALID,
			.host  = "host",
			.port  = 0,
			.path  = "/a/path/for/testing",
		},
		{
			.name  = "all allowed host characters",
			.url   = "gemini://abcdef.ghij.kl.m.nopq.rstv.wxyz.01.23.45.67.89.ABCDEF.GHIJKLMN.O.P.QRSTUVW.XYZ/",
			.valid = VALID,
			.host  = "abcdef.ghij.kl.m.nopq.rstv.wxyz.01.23.45.67.89.ABCDEF.GHIJKLMN.O.P.QRSTUVW.XYZ",
			.port  = 0,
			.path  = "/",
		},
	};

	u = malloc(sizeof(struct gemini_url) + 8192);
	if (!u) {
		fail("memory allocation failed before the tests could get underway!");
		return;
	}
	u->len = 8192;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (cases[i].valid == NOT_VALID) {
			/* invalid case */
			ok(gemini_parse_url_into(cases[i].url, u) != 0,
				"%s '%s' should not parse as a valid Gemini URL", cases[i].name, cases[i].url);
			continue;
		}

		/* valid case */
		ok(gemini_parse_url_into(cases[i].url, u) == 0,
			"%s '%s' should parse as a valid Gemini URL", cases[i].name, cases[i].url);
		is(u->host, cases[i].host,
			"%s '%s' should extract the host part", cases[i].name, cases[i].url);
		is(u->path, cases[i].path,
			"%s '%s' should extract the path part", cases[i].name, cases[i].url);
		if (cases[i].port == 0) {
			cmp_ok(u->port, "==", GEMINI_DEFAULT_PORT,
				"%s '%s' should fall back to the default port", cases[i].name, cases[i].url);
		} else {
			cmp_ok(u->port, "==", cases[i].port,
				"%s '%s' should extract the explicit port", cases[i].name, cases[i].url);
		}
	}

	free(u);
}
