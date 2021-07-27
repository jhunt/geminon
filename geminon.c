#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include <getopt.h>

#include "./gemini.h"

static int echo_handler(struct gemini_request *req, void *_) {
	gemini_request_respond(req, 20, "text/plain");
	gemini_request_write(req, req->url->path, strlen(req->url->path));
	gemini_request_write(req, "\r\n", 2);
	gemini_request_close(req);
	return GEMINI_HANDLER_DONE;
}

int configure(struct gemini_server *server, int argc, char **argv, char **envp) {
	int rc, c, idx;
	char *s1, *s2;

	int handlers = 0;

	int port = 0;

	char *cert = NULL, *key = NULL;

	int nvhosts, cap;
	struct gemini_url **vhosts;

	struct option options[] = {
		{ "echo",            required_argument, NULL, 'E' },
		{ "static",          required_argument, NULL, 'S' },
		{ "bind",            required_argument, NULL, 'b' },
		{ "listen",          required_argument, NULL, 'l' },
		{ "tls-certificate", required_argument, NULL, 'c' },
		{ "tls-key",         required_argument, NULL, 'k' },
		{ 0, 0, 0, 0 },
	};

	nvhosts = 0;
	vhosts = calloc(8, sizeof(struct gemini_url *));
	if (!vhosts) {
		return -1;
	}
	cap = 8;

	/* first, we try the environment */
	cert = getenv("GEMINON_CERTIFICATE");
	if (cert) cert = strdup(cert);

	key = getenv("GEMINON_PRIVATE_KEY");
	if (key) key = strdup(key);

	s1 = getenv("GEMINON_PORT");
	if (s1) {
		port = 0;
		for (s2 = s1; *s2; s2++) {
			if (!isdigit(*s2)) {
				fprintf(stderr, "GEMINON_PORT=%s: not a valid port number (try GEMINON_PORT=1965)\n", s1);
				return -1;
			}
		}
	}

	/* then, we try the command line */
	while (1) {
		idx = 0;
		c = getopt_long(argc, argv, "E:S:b:l:c:k:", options, &idx);
		if (c == -1)
			break;

		switch (c) {
			case 'E':
				handlers++;
				rc = gemini_handle_fn(server, optarg, echo_handler, NULL);
				if (rc != 0) {
					fprintf(stderr, "unable to register echo handler at '%s': %s (error %d)\n", optarg, strerror(errno), errno);
					return -1;
				}
				break;

			case 'S':
				s1 = strdup(optarg);
				s2 = strchr(s1, ':');
				if (!s2) {
					fprintf(stderr, "registering fs handler for '/' urls, served from '%s'\n", s1);
					handlers++;
					rc = gemini_handle_fs(server, "/", s1);
				} else {
					*s2++ = '\0';
					fprintf(stderr, "registering fs handler for '%s' urls, served from '%s'\n", s1, s2);
					handlers++;
					rc = gemini_handle_fs(server, s1, s2);
				}
				free(s1);
				break;

			case 'b':
				if (nvhosts == cap) {
					vhosts = realloc(vhosts, (cap + 8) * sizeof(struct gemini_url *));
					if (!vhosts) {
						return -1;
					}
					cap += 8;
				}
				vhosts[nvhosts] = gemini_parse_url(optarg);
				if (!vhosts[nvhosts]) {
					free(vhosts);
					fprintf(stderr, "%s: not a valid gemini:// URL\n", optarg);
					return -1;
				}
				nvhosts++;
				break;

			case 'l':
				port = 0;
				for (s1 = optarg; *s1; s1++) {
					if (!isdigit(*s1)) {
						fprintf(stderr, "-l %s: not a valid port number (try `-l 1965')\n", optarg);
						return -1;
					}
					port = port * 10 + (*s1 - '0');
				}
				break;

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

	if (optind < argc) {
		fprintf(stderr, "extra arguments found: ");
		while (optind < argc) {
			fprintf(stderr, "%s ", argv[optind++]);
		}
		fprintf(stderr, "\n");
		return -1;
	}

	if (!cert && !key) {
		fprintf(stderr, "you must specify a TLS X.509 certificate and private key via the --tls-certificate=/path and --tls-key=/path options.\n");
		return -1;
	}
	if (!cert) {
		fprintf(stderr, "you must specify a TLS X.509 certificate via the --tls-certificate=/path option.\n");
		return -1;
	}
	if (!key) {
		fprintf(stderr, "you must specify a TLS X.509 private key via the --tls-key=/path option.\n");
		return -1;
	}

	if (handlers == 0) {
		fprintf(stderr, "you must specify at least one handler, via the --echo or --static options\n");
		return -1;
	}

	if (nvhosts > 0) {
		rc = gemini_handle_vhosts(server, vhosts, nvhosts);
		if (rc != 0) {
			return -1;
		}
	}

	port = port ? port : GEMINI_DEFAULT_PORT;
	rc = gemini_bind(server, port);
	if (rc != 0) {
		fprintf(stderr, "unable to listen on *:%d: %s (error %d)\n", port, strerror(errno), errno);
		return -1;
	}

	rc = gemini_tls(server, cert, key);
	if (rc != 0) {
		fprintf(stderr, "tls configuration failed: %s (error %d)\n", strerror(errno), errno);
		return -1;
	}
	printf("loading tls certificate from %s\n", cert);
	printf("loading tls private key from %s\n", key);
	free(cert);
	free(key);

	printf("listening for inbound connections on *:%d\n", port);
	return 0;
}

int main(int argc, char **argv, char **envp) {
	int rc;
	struct gemini_server server;

	rc = gemini_init();
	if (rc != 0) {
		fprintf(stderr, "gemini_init() failed! (e%d: %s)\n", errno, strerror(errno));
		return 1;
	}

	memset(&server, 0, sizeof(server));
	rc = configure(&server, argc, argv, envp);
	if (rc != 0) {
		return 1;
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
