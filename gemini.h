#ifndef __GEMINON_GEMINI_H
#define __GEMINON_GEMINI_H

#define GEMINI_DEFAULT_PORT 1964
#define MAX_GEMINI_REQUEST_SIZE 8192

#define GEMINI_FIXME_LISTEN_BACKLOG 1024

struct gemini_url {
	const char     *host;
	unsigned short  port;
	const char     *path;

	unsigned int len;
	char         buf[];
};

int gemini_parse_url_into(const char *s, struct gemini_url *url);
struct gemini_url * gemini_parse_url(const char *s);

struct gemini_server {
	int          sockfd;
	const char  *docroot;
};

int gemini_bind(struct gemini_server *server);
int gemini_serve(struct gemini_server *server);

#endif
