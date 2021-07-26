#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "./gemini.h"

static int echo_handler(struct gemini_request *req, void *_) {
	gemini_request_respond(req, 20, "text/plain");
	gemini_request_write(req, req->url->path, strlen(req->url->path));
	gemini_request_write(req, "\r\n", 2);
	gemini_request_close(req);
	return GEMINI_HANDLER_DONE;
}

int main(int argc, char **argv, char **envp) {
	int rc;
	struct gemini_server server;
	const struct gemini_url *urls[2];
	memset(&server, 0, sizeof(server));

	rc = gemini_init();
	if (rc != 0) {
		fprintf(stderr, "gemini_init() failed! (e%d: %s)\n", errno, strerror(errno));
		return 1;
	}

	rc = gemini_handle_fs(&server, "/", "t/data");
	if (rc != 0) {
		fprintf(stderr, "gemini_handle_fs() failed! (e%d: %s)\n", errno, strerror(errno));
		return 2;
	}

	rc = gemini_handle_fn(&server, "/", echo_handler, NULL);
	if (rc != 0) {
		fprintf(stderr, "gemini_handle_fn() failed! (e%d: %s)\n", errno, strerror(errno));
		return 2;
	}

	urls[0] = gemini_parse_url("gemini://127.0.0.1:1964/");
	urls[1] = gemini_parse_url("gemini://192.168.129.15:1964/");
	rc = gemini_handle_vhosts(&server, urls, 2);
	if (rc != 0) {
		fprintf(stderr, "gemini_handle_fn() failed! (e%d: %s)\n", errno, strerror(errno));
		return 2;
	}

	rc = gemini_bind(&server, "gemini://localhost:1964/");
	if (rc != 0) {
		fprintf(stderr, "gemini_bind() failed! (e%d: %s)\n", errno, strerror(errno));
		return 3;
	}

	rc = gemini_tls(&server);
	if (rc != 0) {
		fprintf(stderr, "gemini_tls() failed! (e%d: %s)\n", errno, strerror(errno));
		return 3;
	}

	rc = gemini_serve(&server);
	if (rc != 0) {
		fprintf(stderr, "gemini_serve() failed! (e%d: %s)\n", errno, strerror(errno));
		return 4;
	}

	rc = gemini_deinit();
	if (rc != 0) {
		fprintf(stderr, "gemini_deinit() failed! (e%d: %s)\n", errno, strerror(errno));
		return 5;
	}
	return 0;
}
