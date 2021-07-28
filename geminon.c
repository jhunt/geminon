#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#include <getopt.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "./gemini.h"

#include <openssl/x509_vfy.h>
#include <openssl/err.h>

static int echo_handler(const char *prefix, struct gemini_request *req, void *_) {
	gemini_request_respond(req, 20, "text/plain");
	gemini_request_write(req, req->url->path, strlen(req->url->path));
	gemini_request_write(req, "\r\n", 2);
	gemini_request_close(req);
	return GEMINI_HANDLER_DONE;
}

static int cgi_handler(const char *prefix, struct gemini_request *req, void *_root) {
	int rc, pfd[2];
	struct gemini_fs fs;
	char *prog, **argv, **envp;
	pid_t kid;

	fs.root = _root;
	prog = gemini_fs_path(&fs, req->url->path + strlen(prefix));
	if (!prog) {
		return GEMINI_HANDLER_ABORT;
	}

	fprintf(stderr, "executing '%s'...\n", prog);
	argv = calloc(1, sizeof(char *));
	if (!argv) {
		free(prog);
		return GEMINI_HANDLER_ABORT;
	}
	envp = calloc(1, sizeof(char *));
	if (!envp) {
		free(prog); free(argv);
		return GEMINI_HANDLER_ABORT;
	}

	rc = pipe(pfd);
	if (rc != 0) {
		free(prog); free(argv); free(envp);
		return GEMINI_HANDLER_ABORT;
	}

	kid = fork();
	if (kid < 0) {
		fprintf(stderr, "fork failed: %s (error %d)\n", strerror(errno), errno);
		free(prog); free(argv); free(envp);
		return GEMINI_HANDLER_ABORT;
	}

	if (kid != 0) {
		fprintf(stderr, "forked child %d\n", (int)kid);
		/* in parent process; close write end */
		close(pfd[1]);

		fprintf(stderr, "parent: streaming request from child process...\n");
		rc = gemini_request_stream(req, pfd[0], 8192);
		waitpid(kid, NULL, 0);
		fprintf(stderr, "parent: child process exited.\n");

		free(prog); free(argv); free(envp);
		if (rc != 0) {
			return GEMINI_HANDLER_ABORT;
		}
		gemini_request_close(req);
		return GEMINI_HANDLER_DONE;

	} else {
		/* in child process; close read end */
		close(pfd[0]);

		/* redirect stdout to the pipe */
		rc = dup2(pfd[1], 1);
		if (rc < 0) exit(1);
		close(pfd[1]);

		fprintf(stderr, "child: execve'ing '%s'...\n", prog);
		rc = execve(prog, argv, envp);
		fprintf(stderr, "child: execve returned %d\n", rc);
		exit(42);
	}
}

int configure(struct gemini_server *server, int argc, char **argv, char **envp) {
	int rc, c, idx;
	char *s1, *s2, *s3, *prefix;

	int handlers = 0;

	int port = 0;

	char *cert = NULL, *key = NULL;

	X509_STORE *store;
	X509_LOOKUP *lookup;

	int nvhosts, cap;
	struct gemini_url **vhosts;

	struct option options[] = {
		{ "authn",           required_argument, NULL, 'A' },
		{ "echo",            required_argument, NULL, 'E' },
		{ "exec",            required_argument, NULL, 'X' },
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
		c = getopt_long(argc, argv, "A:E:X:S:b:l:c:k:", options, &idx);
		if (c == -1)
			break;

		switch (c) {
			case 'A':
				store = X509_STORE_new();
				if (!store) {
					fprintf(stderr, "unable to create X509_store()\n");
					return 1;
				}

				lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
				if (!store) {
					fprintf(stderr, "unable to create x.509 store lookup\n");
					return 1;
				}

				s1 = strdup(optarg);
				s2 = strchr(s1, ':');
				if (s2) {
					*s2++ = '\0';
					prefix = s1;
				} else {
					s2 = s1;
					prefix = "/";
				}

				for (;;) {
					s3 = strchr(s2, ',');
					if (s3) *s3++ = '\0';
					rc = X509_load_cert_file(lookup, s2, SSL_FILETYPE_PEM);
					if (rc == 0) {
						fprintf(stderr, "%s: unable to load certificate authority certificate\n", s2);
						ERR_print_errors_fp(stderr);
						return 1;
					}
					if (!s3) break;
					s2 = s3;
				}

				rc = gemini_handle_authn(server, prefix, store);
				if (rc != 0) {
					fprintf(stderr, "gemini_handle_authn() failed! (e%d: %s)\n", errno, strerror(errno));
					return 1;
				}
				break;

			case 'E':
				handlers++;
				rc = gemini_handle_fn(server, optarg, echo_handler, NULL);
				if (rc != 0) {
					fprintf(stderr, "unable to register echo handler at '%s': %s (error %d)\n", optarg, strerror(errno), errno);
					return -1;
				}
				break;

			case 'X':
				if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
					fprintf(stderr, "unable to set child signal handler: %s (error %d)\n", strerror(errno), errno);
					return -1;
				}
				s1 = strdup(optarg);
				s2 = strchr(s1, ':');
				if (!s2) {
					fprintf(stderr, "registering exec handler for '/' urls, served from '%s'\n", s1);
					handlers++;
					rc = gemini_handle_fn(server, "/", cgi_handler, s1);
					s1 = NULL; // belongs to gemini now

				} else {
					*s2++ = '\0';
					fprintf(stderr, "registering exec handler for '%s' urls, served from '%s'\n", s1, s2);
					handlers++;
					rc = gemini_handle_fn(server, s1, cgi_handler, strdup(s2));
				}
				free(s1);
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
