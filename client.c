#include "./gemini.h"

#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/ip.h>

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

	if (SSL_CTX_use_certificate_file(client->ssl, cert, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return -2;
	}

	if (SSL_CTX_use_PrivateKey_file(client->ssl, key, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return -3;
	}

	SSL_CTX_set_verify(client->ssl, SSL_VERIFY_NONE, all_ok);
	return 0;
}

struct gemini_response * gemini_client_request(struct gemini_client *client, const char *url) {
	int fd, rc;
	struct gemini_response *res;
	struct sockaddr_in sa;

	rc = inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
	if (rc != 1) {
		return NULL;
	}
	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(1964);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return NULL;
	}

	rc = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		close(fd);
		return NULL;
	}

	res = malloc(sizeof(struct gemini_response));
	if (!res) {
		close(fd);
		return res;
	}

	res->ssl = SSL_new(client->ssl);
	SSL_set_fd(res->ssl, fd);

	if (SSL_connect(res->ssl) != 1) {
		close(fd);
		SSL_free(res->ssl);
		free(res);
		return NULL;
	}

	SSL_write(res->ssl, url, strlen(url));
	SSL_write(res->ssl, "\r\n", 2);
	return res;
}

void gemini_client_free(struct gemini_client *client) {
}
