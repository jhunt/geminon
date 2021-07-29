#include "./gemini.h"

#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/ip.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

static int all_ok(int pre, X509_STORE_CTX *ctx) {
	return 1;
}

int gemini_client_tls(struct gemini_client *client, const char *cert, const char *key)
{
	client->ssl = SSL_CTX_new(TLS_method());
	if (!client->ssl) {
		ERR_print_errors_fp(stderr);
		return -1;
	}

	if (cert && key) {
		if (SSL_CTX_use_certificate_file(client->ssl, cert, SSL_FILETYPE_PEM) <= 0) {
			ERR_print_errors_fp(stderr);
			return -2;
		}

		if (SSL_CTX_use_PrivateKey_file(client->ssl, key, SSL_FILETYPE_PEM) <= 0) {
			ERR_print_errors_fp(stderr);
			return -3;
		}
	}

	SSL_CTX_set_verify(client->ssl, SSL_VERIFY_NONE, all_ok);
	return 0;
}

struct gemini_response * gemini_client_request(struct gemini_client *client, const char *url) {
	int fd, rc;
	struct gemini_response *res;
	struct addrinfo *info, hint, *rp;
	struct gemini_url *u;

	int port;
	char p[6], *service;

	char host[NI_MAXHOST];

	u = gemini_parse_url(url);
	if (!u) {
		return NULL;
	}

	port = u->port;
	service = p + 5;
	*service-- = '\0';
	while (service >= p && port > 0) {
		*service-- = '0' + (port % 10);
		port = port / 10;
	}
	service++;

	memset(&hint, 0, sizeof(hint));
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	fprintf(stderr, "looking up '%s' (port '%s')...\n", u->host, service);
	rc = getaddrinfo(u->host, service, &hint, &info);
	if (rc != 0) {
		free(u);
		fprintf(stderr, "addrinfo error, lookup fail:  %s\n", gai_strerror(rc));
		return NULL;
	}

	fd = -1;
	for (rp = info; rp != NULL; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd < 0) continue;

		getnameinfo(rp->ai_addr, rp->ai_addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
		fprintf(stderr, "connecting to %s (via %s)\n", u->host, host);

		rc = connect(fd, rp->ai_addr, rp->ai_addrlen);
		if (rc == 0) break;

		close(fd);
		fd = -1;
	}
	freeaddrinfo(info);

	if (fd < 0) {
		return NULL;
	}

	res = malloc(sizeof(struct gemini_response));
	if (!res) {
		close(fd);
		return res;
	}

	res->ssl = SSL_new(client->ssl);
	if (!res->ssl) {
		fprintf(stderr, "ssl setup failed\n");
		ERR_print_errors_fp(stderr);
		close(fd);
		free(res);
		return NULL;
	}
	SSL_set_fd(res->ssl, fd);

	if (SSL_connect(res->ssl) != 1) {
		fprintf(stderr, "ssl handshake failed\n");
		ERR_print_errors_fp(stderr);
		close(fd);
		SSL_free(res->ssl);
		free(res);
		return NULL;
	}

	SSL_write(res->ssl, url, strlen(url));
	SSL_write(res->ssl, "\r\n", 2);
	return res;
}

void gemini_client_close(struct gemini_client *client) {
	if (!client)  return;

	if (client->ssl) {
		SSL_CTX_free(client->ssl);
		client->ssl = NULL;
	}
}
