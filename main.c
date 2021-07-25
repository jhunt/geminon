#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "./gemini.h"

int main(int argc, char **argv, char **envp) {
	int rc;
	struct gemini_server server;
	memset(&server, 0, sizeof(server));

	rc = gemini_handle_fs(&server, "/", "t/data");
	if (rc != 0) {
		fprintf(stderr, "gemini_handle_fs() failed! (e%d: %s)\n", errno, strerror(errno));
		return 2;
	}

	rc = gemini_bind(&server, "gemini://localhost:1964/");
	if (rc != 0) {
		fprintf(stderr, "gemini_bind() failed! (e%d: %s)\n", errno, strerror(errno));
		return 3;
	}

	rc = gemini_serve(&server);
	if (rc != 0) {
		fprintf(stderr, "gemini_serve() failed! (e%d: %s)\n", errno, strerror(errno));
		return 4;
	}

	return 0;
}
