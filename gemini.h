#ifndef __GEMINON_GEMINI_H
#define __GEMINON_GEMINI_H

#define DEFAULT_GEMINI_PORT 1965

struct gemini_url {
	const char     *host;
	unsigned short  port;
	const char     *path;

	unsigned int len;
	char         buf[];
};

int gemini_parse_url_into(const char *s, struct gemini_url *url);
struct gemini_url * gemini_parse_url(const char *s);

#endif
