#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <getopt.h>

#include "./gemini.h"

int configure(struct gemini_client *client, int argc, char **argv, char **envp) {
	int rc, c, idx;
	char *cert = NULL, *key = NULL;

	struct option options[] = {
		{ "tls-certificate", required_argument, NULL, 'c' },
		{ "tls-key",         required_argument, NULL, 'k' },
		{ 0, 0, 0, 0 },
	};

	/* first, we try the environment */
	cert = getenv("GEMINI_CLIENT_CERTIFICATE");
	if (cert) cert = strdup(cert);

	key = getenv("GEMINI_CLIENT_PRIVATE_KEY");
	if (key) key = strdup(key);

	/* then, we try the command line */
	while (1) {
		idx = 0;
		c = getopt_long(argc, argv, "c:k:", options, &idx);
		if (c == -1)
			break;

		switch (c) {
			case 'c':
				free(cert);
				cert = strdup(optarg);
				break;

			case 'k':
				free(key);
				key = strdup(optarg);
				break;
		}
	}

	if ((cert && !key) || (!cert && key)) {
		fprintf(stderr, "you must specify both a TLS X.509 certificate and private key via the --tls-certificate=/path and --tls-key=/path options.\n");
		return -1;
	}

	rc = gemini_client_tls(client, cert, key);
	if (rc != 0) {
		fprintf(stderr, "tls configuration failed: %s (error %d)\n", strerror(errno), errno);
		return -1;
	}
	if (cert && key) {
		printf("loading tls certificate from %s\n", cert);
		printf("loading tls private key from %s\n", key);
		free(cert);
		free(key);
	}

	return 0;
}

int main(int argc, char **argv, char **envp) {
	int rc, nreqs;
	struct gemini_client client;
	struct gemini_response *res;

	memset(&client, 0, sizeof(client));
	rc = configure(&client, argc, argv, envp);
	if (rc != 0) {
		return 1;
	}

	for (nreqs = 0; optind < argc; nreqs++, optind++) {
		res = gemini_client_request(&client, argv[optind]);
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
	}
	return 0;
}
