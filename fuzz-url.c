#include "./gemini.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
	struct gemini_url *url;
	char buf[GEMINI_MAX_REQUEST], *p;
	size_t n;

	n = read(0, buf, GEMINI_MAX_REQUEST - 1);
	if (n <= 0) return 1;
	buf[n] = '\0';

	p = strchr(buf, '\r');
	if (!p) p = strchr(buf, '\n');
	if (p) *p = '\0';

	url = gemini_parse_url(buf);
	free(url);
	printf("ok\n");
	return 0;
}
