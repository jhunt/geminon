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
#include <openssl/x509.h>
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

	handler->prefix  = strdup(prefix);
	handler->handler = fn;
	handler->data    = data;

	return gemini_handle(server, handler);
}

static int s_handler_fs(const char *prefix, struct gemini_request *req, void *_root) {
	struct gemini_fs fs;
	int resfd;

	fs.root = _root;
	resfd = gemini_fs_open(&fs, req->url->path + strlen(prefix), O_RDONLY);
	if (resfd < 0) {
		return GEMINI_HANDLER_CONTINUE;
	}

	gemini_request_respond(req, 20, "text/plain");
	if (gemini_request_stream(req, resfd, GEMINI_STREAM_BLOCK_SIZE) < 0) {
		fprintf(stderr, "short write!\n");
	}
	close(resfd);
	gemini_request_close(req);
	return GEMINI_HANDLER_DONE;
}

int gemini_handle_fs(struct gemini_server *server, const char *prefix, const char *root) {
	return gemini_handle_fn(server, prefix, s_handler_fs, strdup(root));
}

static int s_handler_authn(const char *prefix, struct gemini_request *req, void *_store) {
	X509_STORE *store;
	X509_STORE_CTX *ctx;

	if (!req->cert) {
		gemini_request_respond(req, 60, "Certificate Required");
		gemini_request_close(req);
		return GEMINI_HANDLER_DONE;
	}

	store = _store;
	ctx = X509_STORE_CTX_new();
	X509_STORE_CTX_init(ctx, store, req->cert, NULL);

	if (X509_verify_cert(ctx) != 1) {
		ERR_print_errors_fp(stderr);
		gemini_request_respond(req, 61, "Unauthorized");
		gemini_request_close(req);
		return GEMINI_HANDLER_DONE;
	}

	return GEMINI_HANDLER_CONTINUE;
}

int gemini_handle_authn(struct gemini_server *server, const char *prefix, X509_STORE *store) {
	return gemini_handle_fn(server, prefix, s_handler_authn, store);
}

struct _vhosts {
	struct gemini_url ** urls;
	int n;
};

static void s_vhosts_free(struct _vhosts *x) {
	int i;
	for (i = 0; i < x->n; i++) {
		free(x->urls[i]);
	}
	free(x->urls);
	free(x);
}

static int s_handler_vhosts(const char *prefix, struct gemini_request *req, void *_vhosts) {
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

int gemini_handle_vhosts(struct gemini_server *server, struct gemini_url ** urls, int n) {
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

int gemini_bind(struct gemini_server *server, int port) {
	int fd, rc, v;
	struct sockaddr_in sa;

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
	sa.sin_port        = htons(port);
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
	size_t ntotal = 0, nread;

	while ((SSL_read_ex(ssl, dst+ntotal, len-1-ntotal, &nread)) == 1) {
		ntotal += nread;
		*(dst+ntotal) = '\0';
		if (strstr(dst, end) != NULL) {
			return ntotal;
		}
	}
	return -1;
}

static int _tls_verify(int preverify_ok, X509_STORE_CTX *ctx) {
	return 1;
}

int gemini_tls(struct gemini_server *server, const char *cert, const char *key) {
	server->ssl = SSL_CTX_new(TLS_method());
	if (!server->ssl) {
		ERR_print_errors_fp(stderr);
		return -1;
	}

	if (SSL_CTX_use_certificate_file(server->ssl, cert, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return -2;
	}

	if (SSL_CTX_use_PrivateKey_file(server->ssl, key, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return -3;
	}

	SSL_CTX_set_verify(server->ssl, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, _tls_verify);
	return 0;
}

int gemini_serve(struct gemini_server *server) {
	int rc, handled;
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
			SSL_free(req.ssl);
			close(req.fd);
			continue;
		}

		req.cert = SSL_get_peer_certificate(req.ssl);

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

		handled = 0;
		for (handler = server->first; handler; handler = handler->next) {
			if (strlen(req.url->path) < strlen(handler->prefix)) {
				continue;
			}
			if (strncmp(req.url->path, handler->prefix, strlen(handler->prefix)) != 0) {
				continue;
			}

			rc = handler->handler(handler->prefix, &req, handler->data);
			if (rc == GEMINI_HANDLER_CONTINUE) {
				continue;
			}
			if (rc == GEMINI_HANDLER_DONE) {
				handled = 1;
				break;
			}

			if (rc == GEMINI_HANDLER_ABORT) {
				gemini_request_respond(&req, 59, "Internal Error");
				gemini_request_close(&req);
				handled = 1;
				break;
			}
		}

		if (!handled) {
			fprintf(stderr, "[gemini_serve] not handled; trying fallback handler...\n");
			gemini_request_respond(&req, 51, "Not Found");
			gemini_request_close(&req);
		}

		server->requests++;
		if (server->max_requests > 0 && server->requests > server->max_requests) {
			return 0;
		}
	}

	return -1;
}

void gemini_server_close(struct gemini_server *server) {
	struct gemini_handler *handler, *next;

	SSL_CTX_free(server->ssl);

	for (handler = server->first; handler; handler = next) {
		next = handler->next;

		if (handler->handler == s_handler_vhosts) {
			s_vhosts_free(handler->data);
		} else if (handler->handler == s_handler_authn) {
			X509_STORE_free(handler->data);
		} else {
			free(handler->data);
		}
		free(handler->prefix);
		free(handler);
	}
}
