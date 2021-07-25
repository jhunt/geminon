#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "./gemini.h"

int main(int argc, char **argv, char **envp) {
	int rc;
	struct gemini_server server;
	server.root = "t/data";

	server.url = gemini_parse_url("gemini://localhost:1964/");
	if (!server.url) {
		fprintf(stderr, "gemini_parse_url() failed: e%d: %s)\n", errno, strerror(errno));
		return 1;
	}

	rc = gemini_bind(&server);
	if (rc != 0) {
		fprintf(stderr, "gemini_bind() failed! (e%d: %s)\n", errno, strerror(errno));
		return 2;
	}

	rc = gemini_serve(&server);
	if (rc != 0) {
		fprintf(stderr, "gemini_serve() failed! (e%d: %s)\n", errno, strerror(errno));
		return 3;
	}

	return 0;
}
