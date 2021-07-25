#include "./gemini.h"

#include <openssl/ssl.h>


int gemini_init() {
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();
	return 0;
}

int gemini_deinit() {
	EVP_cleanup();
	return 0;
}
