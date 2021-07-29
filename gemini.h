#ifndef __GEMINON_GEMINI_H
#define __GEMINON_GEMINI_H

#include <sys/types.h>
#include <openssl/ssl.h>

/* Handlers should return GEMINI_HANDLER_CONTINUE to have the core continue
   its search for an appropriate handler. Authentication handlers make
   extensive use of this return value. */
#define GEMINI_HANDLER_CONTINUE -1

/* Handlers that have responded and closed the request should return
   GEMINI_HANDLER_DONE to let the core know it can stop processing it. */
#define GEMINI_HANDLER_DONE      0

/* Handlers that want to immediately abort the connection because of an
   error condition, but do not want to directly respond to the client should
   return GEMINI_HANDLER_ABORT.  This is for emergency use only. */
#define GEMINI_HANDLER_ABORT     1

/* The default Gemini port, per the protocol.
   We assume this port for URLs that do not specify an explicit port number.
 */
#define GEMINI_DEFAULT_PORT 1965

/* The maximum size (in bytes) of an inbound request line */
#define GEMINI_MAX_REQUEST 8192

/* The maximum size (in bytes) of a response status line */
#define GEMINI_MAX_RESPONSE 256

/* The maximum size (in bytes) of a single filesystem component */
#define GEMINI_MAX_PATH    2048

/* How large should the server's backlog of unaccepted connections be? */
#define GEMINI_LISTEN_BACKLOG    1024

/* Preferred block size to use for streaming fd-to-fd copies */
#define GEMINI_STREAM_BLOCK_SIZE 8192

/* Before you can use the geminon library, either as a server handling
   requests from clients, or as a client making said requests, you have to
   initialize some shared, static, global state.

   This is done by a single call to gemini_init().

   It's not a good idea to call this more than once.
 */
int gemini_init();

/* If you care about memory leaks, and aren't just going to fall off the end
   of the main() function, you'll need to call gemini_deinit() after you are
   done interacting with the geminon library.

   It frees up the global state that gemini_init() set up.
 */
int gemini_deinit();

/* A gemini_client can make protocol requests to any number of Gemini
   servers on the network.  It also allows you to specify what TLS client
   credentials you want to use for said requests (although that is entirely
   optional).

   Normally, you'll want to allocate and zero out a bare gemini_client
   structure, like this:

       struct gemini_client my_client;
       memset(&my_client, 0, sizeof(my_client));

   There is no allocation logic, so if you are more at home on the heap,
   feel free to use malloc(3):

       struct gemini_client *my_client;
       my_client = malloc(sizeof(*my_client));

  Regardless, you will eventually need to call gemini_client_close() to free
  up interior resources like TLS contexts, that are set up by other
  gemini_client_* functions.

       struct gemini_client my_client;
       memset(&my_client, 0, sizeof(my_client));
       // ... do something ...
       gemini_client_close(&my_client);

  For heap-allocated client objects, don't forget that free(3) is YOUR
  responsibility; you malloc(3)'d it, you get to free(3) it:

       struct gemini_client *my_client;
       my_client = malloc(sizeof(*my_client));
       // ... do something ...
       gemini_client_close(my_client);
       free(my_client);

 */
struct gemini_client {
	SSL_CTX *ssl; /* TLS parameters (client certificate / private key) */
};

/* A gemini_response is what gets sent in reply to an request.  Mostly, this
   contains internal state variables that are of little interest to callers
   directly.  The most interesting thing you can do with a response is read
   from it.
 */
struct gemini_response {
	int  fd;   /* underlying file descriptor; for reading */
	SSL *ssl;  /* OpenSSL descriptor, wrapping fd, for encrytion */
};

/* The Gemini protocol explicitly requires that servers always provide X.509
   certificates and communicate solely over modern(-ish) TLS.  However, it
   does not require the same of clients.

   For clients that do wish to present X.509 certificates for identity
   validation, the gemini_client_tls() function exists.  You provide it with
   the paths (on-disk) to your PEM-encoded public certificate and private
   key, and it will configure the client accordingly.

   Note that this is not reversible; subsequent calls to gemini_client_tls()
   will either fail spectacularly, or cause insidious memory leaks.  If you
   want to do different certs for different servers, you'll have to maintain
   separate client objects.
 */
int gemini_client_tls(struct gemini_client *client, const char *cert, const char *key);

/* Makes a request.

   It's a bit more complicated than that, but that's the point of
   abstractions, isn't it?  The gemini_client_request() function uses a
   client (and it's TLS configuration, if any) to make a request to the
   identified endpoint, a gemini:// URL.

   The returned gemini_response structure can be read from buffer-wise
   (using gemini_request_read()), or streamed wholesale (thanks to the
   gemini_request_stream() function).
 */
struct gemini_response * gemini_client_request(struct gemini_client *client, const char *url);

/* When you're finished with a client object, call gemini_client_close() to
   relinquish any resources it was holding onto.  Mostly this is TLS stuff,
   but it doesn't hurt to call it even if you haven't called
   gemini_client_tls().
 */
void gemini_client_close(struct gemini_client *client);

/* Reads up to n octets from the responding Gemini server, into the
   caller-provided buffer.  Returns the number of octets read, 0 on
   end-of-file, and negative values to signal errors.

   This function hews to the semantics of read(2) on purpose.
 */
ssize_t gemini_response_read(struct gemini_response *res, void *buf, size_t n);

/* Copies all of the response from the responding Gemini server into the
   given output file descriptor (fd), until reaching end-of-file.  The block
   parameter governs how big of a buffer to allocate for the copy.  I'm
   partial to 8192.  This also controls how large of a read is attempted at
   a given shot.

   Returns 0 on success, and negative values to incdicate failures.
 */
int gemini_response_stream(struct gemini_response *res, int fd, size_t block);

/* Deallocate and release all resources attached to a response.  This
   includes the TLS bits as well as the underlying file descriptor for the
   connected socket.  The remote end is still open (pending timeouts) until
   this function is called.

   It is safe (if a bit sloppy) to call this function more than once for the
   same response object.
 */
void gemini_response_close(struct gemini_response *res);

/* A gemini_url gives you access to the parsed components of a gemini://
   uniform resource locator.  Specifically, the host, port, and path
   components can be accessed individually.

   Most of the time, you're going to want to parse a string representation
   of a URL into this structure using gemini_parse_url_into("...", &url) -
   if you already have a struct you want to use - or gemini_parse_url().

   The parsing routines work by slowly building up a buffer of
   NULL-separated component strings, as the full URL is parsed.  This is the
   buf attribute.  The len attribute tracks the size that the caller
   allocated.

   To allocate one of these by hand:

       struct gemini_url *url = malloc(
         sizeof(struct gemini_url) +
         SIZE_OF_BUFFER_SPACE
       );
       url->len = SIZE_OF_BUFFER_SPACE;

   Where SIZE_OF_BUFFER_SPACE is how much space to give the internal buffer.
   An easier (and less error-prone) approach is to use gemini_new_url()
instead:

       struct gemini_url *url;
       url = gemini_new_url(SIZE_OF_BUFFER_SPACE)

 */
struct gemini_url {
	const char     *host;
	unsigned short  port;
	const char     *path;

	unsigned int len;
	char         buf[];
};

/* Allocates a new gemini_url structure, with the appropriately sized
   internal buffer, and returns a pointer to it.  Caller is responsible for
   freeing the structure when it is no longer needed; free(3) will suffice
   for this.
 */
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
char * gemini_fs_path(struct gemini_fs *fs, const char *file);

/* A gemini_request is used by the server-side handlers to route and process
   inbound requests from Gemini clients.  It contains things like the
   requested URL (fully parsed) and the client X.509 certificate (if any).
 */
struct gemini_request {
	int  fd;  /* underlying file descriptor of the accept(2)'d socket */
	SSL *ssl; /* TLS bits, wrapping fd, to provide encrypted comms */

	struct gemini_url *url; /* requested URL, including host, port, and path */
	X509 *cert;             /* The client X.509 certificate, if one was sent */
};

/* A gemini_handler is a specific type of function that is used to provide
   semi-dynamic dispatch request handling.  If the core routing logic
   decides that a given handler should weigh in on (and possible respond to)
   an inbound request, this is the function that gets executed.

   Each handler gets three things: the prefix it was registered against, the
   request itself (containing the URL and client certificate), and a
   handler-specific bit of memory (user data).

   The prefix allows the same handler function to be registered at multiple
   points in the server's URL space.

   The user data allows the same handler function logic to do different
   things based on caller needs.  For example, a generic cgi handler could
   be given a base path to look for executable server-side scripts in.
 */
typedef int (*gemini_handler)(const char *prefix, struct gemini_request *, void *);

/* A gemini_handler structure wraps up all of the configuration of a single
   "handler" (or, if you prefer, "middleware") registered with a gemini
   server.  Each handler structure is part of a forward-only linked list,
   and tracks the three parts of a handler:

     1. The registered prefix
     2. The handler function (a gemini_handler)
     3. User data provided when the handler was registered.

  In reality, no one outside of the gemini_server implementation cares about
  this structure.  It may be removed (or hidden) in a future release.
 */
struct gemini_handler {
	struct gemini_handler *next; /* linked list forward pointer */

	char           *prefix;  /* registered URL path prefix */
	gemini_handler  handler; /* the gemini_handler with all the logic */
	void           *data;    /* Caller-supplied data (passed to handler) */
};

/* A gemini_server ties together a whole bunch of configuration, handlers,
   TLS parameters, etc., and allows you to write a dynamic server that
   responds to Gemini protocol requests with Gemini protocol responses.

   You can make a new gemini_server by zeroing out the allocated struct:

       struct gemini_server my_server;
       memset(&my_server, 0, sizeof(my_server));

   From there, you'll associate the TLS X.509 public certificate to present
   to connecting clients, and its private key (both as PEM files):

       gemini_tls(&my_server, "path/to/cert.pem", "path/to/key.pem");

   The majority of your work will be wound up in writing handlers for the
   gemini_server to evaluate and dispatch to.  There are a few built-in
   handlers, for convenience.  To serve up static files:

       gemini_handle_fs(&my_server, "/", "/srv/gemini");

   To bind a listening socket and serve clients:

       gemini_bind(&my_server, GEMINI_DEFAULT_PORT);
       gemini_serve(&my_server);

   Note that as of right now, you can only bind a single socket.
 */
struct gemini_server {
	int      sockfd; /* underlying (bound) socket descriptor */
	SSL_CTX *ssl;    /* TLS configuration (cert / key to use) */

	/* Some testing scenarios require that a gemini server only live long
	   enough to handle a small, finite number of requests.  These two
	   attributes control that behavior.  As requests are handled, the
	   requests attribute is incremented.  If it ever exceeds a (non-zero)
	   max_requests threshold, the server stops listening and shuts down,
	   returning from gemini_serve().
	 */
	unsigned int requests, max_requests;

	/* Handlers are registered in FIFO order.  For convenience, and to avoid
	   having to traverse the handlers list to append to the end of it, we
	   track both the first and last handler in the list.
	 */
	struct gemini_handler *first, *last;
};

/* Send the Gemini response status line to the client.

   Gemini protocol statuses consist of a two-digit status code and a
   status message of unspecified meaning (or length).  This library places
   an upper limit of GEMINI_MAX_RESPONSE bytes on the entire status line,
   including the trailing carriage return / line feed.

   Returns the number of octets written to the socket on success, and a
   negative value on failure.
 */
int gemini_request_respond(struct gemini_request *req, int status, const char *meta);

/* Writes up to n octets from the caller-provided buffer to the client.

   We don't consider it a problem if the remote end would only accept a
   subset of our write.  gemini_request_write() returns the number of octets
   actually written, in the same spirit as write(2).

   If an error occurs (client disconnects, the Internet blows up, etc.), a
   negative value is returned.
 */
ssize_t gemini_request_write(struct gemini_request *req, const void *buf, size_t n);

/* Stream a file descriptor to the client, copy all of its contents until
   either the everything has been written, or an error occurs.  The block
   argument controls (a) how much we try to send in a single write(2) call,
   and (b) how big of a transfer buffer to heap-allocate.

   Returns 0 on success, and negative on failure.
 */
int gemini_request_stream(struct gemini_request *req, int fd, size_t block);

/* When you're all done writing to the client, call gemini_request_close().
   Doing so releases TLS resources associated with the request, frees the
   memory devoted to the parsed request URL, and closes the underlying
   connection descriptor.

   It's just good personal hygeine.
 */
void gemini_request_close(struct gemini_request *req);

/* Register a handler, using a pre-populated gemini_handler struct with all
   of the details.  From a memory perspective, there are very specific rules
   that must be followed to avoid double-frees and memory corruption:

     1. handler->prefix must be heap-allocated.  It will be free(3)'d
     2. handler->data must be heap-allocated.  It wil also be free(3)'d
     3. handler itself must be heap-allocated... because free(3)

  As we see more in-the-wild usage, these restrictions may change.
 */
int gemini_handle(struct gemini_server *server, struct gemini_handler *handler);

/* Register a handler.  This is the most popular way of adding dynamism to
   your server program.  The handler will fire for all URLs at or under the
   given prefix.  The handler function itself will received the passed data
   pointer, and can interpret it however it needs.

   The data pointer must be heap-allocated, because it will be free(3)'d
   when a gemini_server_close() is called.  This may change.
 */
int gemini_handle_fn(struct gemini_server *server, const char *prefix, gemini_handler fn, void *data);

/* Register a static-files handler.  URLs at or under the given prefix will
   be re-interpreted as being relative to root instead, and those files (if
   they exist) will be sent to requesting clients.  If no matching files are
   found, the handler will skip the request, letting future handlers
   attempt to do something useful.

   Unlike other handler registration functions, gemini_handle_fs does not
   need root to be heap-allocated; it will allocate a copy via strdup(3).
   This supports the following common case:

       gemini_handle_fs(&server, "/", "/srv/files");

 */
int gemini_handle_fs(struct gemini_server *server, const char *prefix, const char *root);

/* Register an authn handler, which will verify that all requests to URLs at
   or below prefix are made with a client X.509 certificate signed by a
   certificate authority that has been pre-laoded into the store.  How that
   is done is beyond the scope of this documentation.

   The gemini_server will take over ownership of the passed X509_STORE
   pointer, and will handle freeing it (via OpenSSL conventions) when
   appropriate.

   Note that the authn handler never responds succesfully to any request.
   If no certificate is found, a status 60 is returned.  If an unverified
   client certificate is presented, a tatus 61 is returned.  Otherwise, the
   handler allows the core to continue searching for an appropriate handler.

   For that reason, you usually want to register these early on.
 */
int gemini_handle_authn(struct gemini_server *server, const char *prefix, X509_STORE *store);

/* Register a vhosts handler, which will verify that the request is for one
   of the n given gemini:// protocol URLs.  If not, the request will be
   handled with a status 51.  Otherwise, the core will continue searching
   for an appropriate handler.

   The gemini_server will take over ownership of the passed urls parameter,
   which must be heap-allocated.
 */
int gemini_handle_vhosts(struct gemini_server *server, struct gemini_url **urls, int n);

/* Bind a socket to the given Gemini URL (path notwithstanding) so that a
   future call to gemini_serve() can listen and accept connections.  The
   socket will be set to REUSEADDR, to ensure quick startup of servers.
 */
int gemini_bind(struct gemini_server *server, int port);


/* The Gemini protocol explicitly requires that servers always provide X.509
   certificates and communicate solely over modern(-ish) TLS.

   The gemini_tls() function is how you associate the X.509 server
   certificate, and its private key, with the machinery that will accept
   inbound connections and negotiate transport security.  You provide it
   with the paths (on-disk) to your PEM-encoded public certificate and
   private key, and it will configure the server accordingly.
 */
int gemini_tls(struct gemini_server *server, const char *cert, const char *key);

/* Listen to the socket created by a gemini_bind() against the passed server
   object, and service clients as they connect.
 */
int gemini_serve(struct gemini_server *server);

/* When you're finished with a server object, call gemini_server_close() to
   relinquish any resources it was holding onto.  Mostly this is TLS stuff,
   and bound socket descriptors, but it doesn't hurt to call it even if you
   haven't called gemini_tls(), gemini_bind(), or gemini_serve().
 */
void gemini_server_close(struct gemini_server *server);

#endif
