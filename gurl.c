#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "./gemini.h"

int main(int argc, char **argv, char **envp) {
	int rc;
	struct gemini_client client;
	struct gemini_response *res;

	memset(&client, 0, sizeof(client));
	rc = gemini_client_tls(&client, "cert.pem", "key.pem");
	if (rc != 0) {
		fprintf(stderr, "gemini_client_tls() failed! (e%d: %s)\n", errno, strerror(errno));
		return 1;
	}

	res = gemini_client_request(&client, "gemini://127.0.0.1:1964/echo/hello/hello/there");
	if (!res) {
		fprintf(stderr, "gemini_client_request() failed! (e%d: %s)\n", errno, strerror(errno));
		return 1;
	}

	rc = gemini_response_stream(res, 1, 8192);
	if (rc != 0) {
		fprintf(stderr, "gemini_response_stream() failed! (e%d: %s)\n", errno, strerror(errno));
		return 1;
	}
	gemini_response_close(res);
	return 0;
}
