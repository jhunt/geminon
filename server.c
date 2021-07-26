#include "./gemini.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <fcntl.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

int gemini_handle(struct gemini_server *server, struct gemini_handler *handler) {
	handler->next = NULL;

	if (!server->first) server->first      = handler;
	if ( server->last ) server->last->next = handler;
	server->last = handler;

	return 0;
}

int gemini_handle_fn(struct gemini_server *server, const char *prefix, gemini_handler fn, void *data) {
	struct gemini_handler *handler;

	handler = malloc(sizeof(struct gemini_handler));
	if (!handler) {
		return -1;
	}

	handler->prefix  = prefix;
	handler->handler = fn;
	handler->data    = data;

	return gemini_handle(server, handler);
}

static int s_handler_fs(struct gemini_request *req, void *_fs) {
	struct gemini_fs *fs;
	int resfd;

	fs = _fs;
	resfd = gemini_fs_open(fs, req->url->path, O_RDONLY);
	if (resfd < 0) {
		return GEMINI_HANDLER_CONTINUE;
	}

	gemini_request_respond(req, 20, "text/plain");
	if (gemini_request_stream(req, resfd, 8192) < 0) {/* FIXME magic number */
		fprintf(stderr, "short write!\n");
	}
	close(resfd);
	gemini_request_close(req);
	return GEMINI_HANDLER_DONE;
}

int gemini_handle_fs(struct gemini_server *server, const char *prefix, const char *root) {
	struct gemini_fs *fs;

	fs = malloc(sizeof(struct gemini_fs));
	if (!fs) {
		return -1;
	}
	fs->root = root;

	if (gemini_handle_fn(server, prefix, s_handler_fs, fs) != 0) {
		free(fs);
		return -1;
	}

	return 0;
}

struct _vhosts {
	const struct gemini_url **urls;
	int n;
};

static int s_handler_vhosts(struct gemini_request *req, void *_vhosts) {
	struct _vhosts *vhosts;
	int i;

	vhosts = _vhosts;
	for (i = 0; i < vhosts->n; i++) {
		if (strcmp(vhosts->urls[i]->host,   req->url->host) == 0
		 &&        vhosts->urls[i]->port == req->url->port) {
			return GEMINI_HANDLER_CONTINUE;
		}
	}

	gemini_request_respond(req, 53, "Not Found");
	gemini_request_close(req);
	return GEMINI_HANDLER_DONE;
}

int gemini_handle_vhosts(struct gemini_server *server, const struct gemini_url **urls, int n) {
	struct _vhosts *vhosts;

	vhosts = malloc(sizeof(struct _vhosts));
	if (!vhosts) {
		return -1;
	}

	vhosts->urls = urls;
	vhosts->n = n;

	if (gemini_handle_fn(server, "/", s_handler_vhosts, vhosts) != 0) {
		free(vhosts);
		return -1;
	}

	return 0;
}

int gemini_bind(struct gemini_server *server, const char *url) {
	int fd, rc, v;
	struct sockaddr_in sa;

	server->url = gemini_parse_url("gemini://localhost:1964/");
	if (!server->url) {
		return -1;
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return fd;
	}

	v = 1;
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
	if (rc < 0) {
		close(fd);
		return rc;
	}

	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(server->url->port);
	sa.sin_addr.s_addr = INADDR_ANY;

	rc = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		close(fd);
		return rc;
	}

	rc = listen(fd, GEMINI_LISTEN_BACKLOG);
	if (rc < 0) {
		close(fd);
		return rc;
	}

	server->sockfd = fd;
	return 0;
}

static ssize_t s_readto(SSL *ssl, char *dst, size_t len, const char *end) {
	size_t i, j, ntotal = 0, nread;

	while ((SSL_read_ex(ssl, dst+ntotal, len-1-ntotal, &nread)) == 1) {
		ntotal += nread;
		*(dst+ntotal) = '\0';
		if (strstr(dst, end) != NULL) {
			return ntotal;
		}
	}
	return -1;
}

int gemini_tls(struct gemini_server *server) {
	server->ssl = SSL_CTX_new(TLS_method());
	if (!server->ssl) {
		ERR_print_errors_fp(stderr);
		return -1;
	}

	if (SSL_CTX_use_certificate_file(server->ssl, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return -2;
	}

	if (SSL_CTX_use_PrivateKey_file(server->ssl, "key.pem", SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return -3;
	}

	return 0;
}

int gemini_serve(struct gemini_server *server) {
	int handled;
	ssize_t n;
	char *p, buf[GEMINI_MAX_REQUEST];
	struct gemini_request req;
	struct gemini_handler *handler;

	memset(&req, 0, sizeof(req));
	while ((req.fd = accept(server->sockfd, NULL, NULL)) != -1) {
		fprintf(stderr, "[gemini_serve] accepted inbound connection on fd %d\n", req.fd);

		req.ssl = SSL_new(server->ssl);
		SSL_set_fd(req.ssl, req.fd);
		if (SSL_accept(req.ssl) <= 0) {
			close(req.fd);
			continue;
		}

		n = s_readto(req.ssl, buf, sizeof(buf), "\r\n");
		if (n <= 0) {
			fprintf(stderr, "[gemini_serve] received error while reading from connection on fd %d\n", req.fd);
			gemini_request_close(&req);
			continue;
		}

		p = strstr(buf, "\r\n");
		assert(p); *p = '\0';

		fprintf(stderr, "[gemini_serve] checking url '%s'\n", buf);
		req.url = gemini_parse_url(buf);
		if (!req.url) {
			fprintf(stderr, "[gemini_serve] '%s' is an invalid gemini:// protocol url\n", buf);
			gemini_request_respond(&req, 50, "Bad URL");
			gemini_request_close(&req);
			continue;
		}

		/* walk the handlers */
		handled = 0;
		for (handler = server->first; handler; handler = handler->next) {
			/* FIXME check for prefix match! */
			if (handler->handler(&req, handler->data) == GEMINI_HANDLER_DONE) {
				handled = 1;
				break;
			}
		}
		if (!handled) {
			fprintf(stderr, "[gemini_serve] not handled; trying fallback handler...\n");
			gemini_request_respond(&req, 51, "Not Found");
		}
		gemini_request_close(&req);
	}

	return -1;
}
