#ifndef __GEMINON_GEMINI_H
#define __GEMINON_GEMINI_H

#define GEMINI_DEFAULT_PORT 1964
#define MAX_GEMINI_REQUEST_SIZE 8192
#define GEMINI_MAX_PATH         2048

#define GEMINI_FIXME_LISTEN_BACKLOG 1024
#define GEMINI_IOCOPY_BLOCK_SIZE    8192

/* A gemini_url gives you access to the parsed components
   of a gemini:// uniform resource locator.  Specifically,
   the host, port, and path components can be accessed
   individually.

   Most of the time, you're going to want to parse a string
   representation of a URL into this structure using either
   gemini_parse_url_into("...", &url) - if you already have
   a struct you want to use - or gemini_parse_url().

   The parsing routines work by slowly building up a buffer
   of NULL-separated component strings, as the full URL is
   parsed.  This is the buf attribute.  The len attribute
   tracks the size that the caller allocated.

   To allocate one of these by hand:

       struct gemini_url *url = malloc(
         sizeof(struct gemini_url) +
         SIZE_OF_BUFFER_SPACE
       );
       url->len = SIZE_OF_BUFFER_SPACE;

   Where SIZE_OF_BUFFER_SPACE is how much space to give
   the internal buffer.  An easier (and less error-prone)
   approach is to use gemini_new_url() instead:

       struct gemini_url *url =
         gemini_new_url(SIZE_OF_BUFFER_SPACE)

 */
struct gemini_url {
	const char     *host;
	unsigned short  port;
	const char     *path;

	unsigned int len;
	char         buf[];
};

/* Allocates a new gemini_url structure, with the appropriately
   sized internal buffer, and returns a pointer to it.  Caller
   is responsible for freeing the structure when it is no longer
   needed; free(3) will suffice for this. */
struct gemini_url * gemini_new_url(unsigned int size);

/* Parse a gemini URL string into a (pre-allocated) gemini_url
   structure.  If the internal buffer of the gemini_url struct
   is insufficient to house the host and path components, an
   error will be returned.

   Returns 0 on success, or a negative value on error. */
int gemini_parse_url_into(const char *s, struct gemini_url *url);

/* Parse a gemini URL string into a newly-allocated gemini_url
   structure.  The internal buffer of the new structure will be
   properly sized.  This is roughly equivalent to:

       struct gemini_url *url = gemini_new_url(strlen(s));
       gemini_parse_url(s, url);

   This means that the caller is responsible for freeing the
   returned structure, via free(3).  If the URL string cannot
   be parsed, a NULL is returned and no memory is consumed,
   although a heap allocation / free may still occur. */
struct gemini_url * gemini_parse_url(const char *s);

struct gemini_fs {
	const char *root;
};

char * gemini_fs_resolve(const char *file);
int gemini_fs_open(struct gemini_fs *fs, const char *file, int flags);

struct gemini_server {
	int                sockfd;
	const char        *root;
	struct gemini_url *url;
};

int gemini_bind(struct gemini_server *server);
int gemini_serve(struct gemini_server *server);

#endif
