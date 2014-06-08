/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Sandra Ros Hrefnu Jonsdottir, sandrarj13@ru.is
 *     Grettir Olafsson, grettir10@ru.is
 * 
 * This proxy server is multithreaded, it can handle multiple requests simulatenously. The main thread
 * binds and listens for connections on the supplied port and creates and delegates the request to a thread.
 * The worker thread then reads the request and builds it's own copy of the request to pass along to the 
 * origin-server that the client wants to connect to. The thread then accepts the response from the origin-server
 * and routes it back to the client.
 *
 * Handling for chunked responses is not implemented yet, so it is still pretty slow. Although faster than a 
 * sequential server.
 *
 * Every request that is completed is logged in proxy.log in the servers root directory.
 * Log format:
 *       <date> <time> <time-zone>: <client addr> <hostname> <bytes received>
 *       Thu 27 Mar 2014 22:52:08 GMT: 127.0.0.1 http://ymsir.com/ 13328
 * 
 * TODO: Handle chunked responses from origin-server.
 * TODO: Look at handling different methods? Not high priority. 
 *
 */ 

#include "csapp.h"

#define MAXTHREADS 300

// Mutex locks for logging and for thread safety of open_clientfd_ts() when it calls gethostbyname().
pthread_mutex_t log_lock;
pthread_mutex_t ts_lock;

/*
 * Function prototypes
 */
void *doit(void *argp);//int connfd, struct sockaddr_in *clientaddr);
int parse_uri(char *uri, char *hostname, char *pathname, int  *port);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);

void Pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
void Pthread_mutex_lock(pthread_mutex_t *mutex);
void Pthread_mutex_unlock(pthread_mutex_t *mutex);

int open_clientfd_ts(char *hostname, int port);
int Open_clientfd_ts(char *hostname, int port);

/*
 * Struct to pass arguments to threads.
 */
typedef struct {
	int thread_id;
	int connfd;
	struct sockaddr_in clientaddr;
} thread_arg;

/* 
 * main - Main routine for the proxy program 
 *
 * Listens on the supplied port and accepts connections. Creates a new
 * thread to handle each HTTP request.
 */
int main(int argc, char **argv)
{
	int listenfd, connfd, port, clientlen;
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *haddrp;
	int thread_id = 0;
	pthread_t tid[MAXTHREADS];
	thread_arg t[MAXTHREADS];

	Signal(SIGPIPE, SIG_IGN);
	/* Check arguments */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
	}
	port = atoi(argv[1]);


	Pthread_mutex_init(&log_lock, NULL);
	Pthread_mutex_init(&ts_lock, NULL);

	listenfd = Open_listenfd(port); // Start listening on the port supplied.
	while(1)
	{
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);

		// Determine the domain name and IP address of the client
		hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		haddrp = inet_ntoa(clientaddr.sin_addr);
		printf("server connected to %s (%s)\n", hp->h_name, haddrp);
		// Fill thread_args struct to be used
		t[thread_id].thread_id = thread_id;
		t[thread_id].connfd = connfd;
		t[thread_id].clientaddr = clientaddr;
		// Create thread.
		Pthread_create(&tid[thread_id], NULL, doit, (void *)&t[thread_id]);

		thread_id = (thread_id + 1) % MAXTHREADS;
	}
	exit(0);
}

/*
 * doit - Handle HTTP requests
 *
 * Thread function. Receives a struct with arguments, thread_id (although I realise now that's totally
 * useless because of Pthread_self()..) as well as a connfd file descriptor from the requesting client
 * and a sockaddr_in struct with the client's address information.
 *
 * It's a bit of a long one, a good next step (after getting the chunked responses handled) would be to
 * break it up into smaller functions.
 *
 * We check the request, parse the uri and build an HTTP request for the origin-server. Open a connection
 * to the origin-server and send the request and then we forward the response back to the client.
 * Log the request in proxy.log in the end.
 */
void *doit(void *argp)//int connfd, struct sockaddr_in *clientaddr)
{
	Pthread_detach(Pthread_self());
	thread_arg *args;
	args = (thread_arg *) argp;
	int thread_id = args->thread_id;
	int connfd = args->connfd;
	struct sockaddr_in clientaddr = args->clientaddr;

	int clientfd, port;
    int chunked = 0;
	size_t n, size = 0;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char hostname[MAXLINE], pathname[MAXLINE];
	char client_buf[MAXLINE], client_message[MAXBUF];
	char logstring[MAXLINE];
	rio_t rio, client_rio;
	
	// Read request line and headers
	Rio_readinitb(&rio, connfd);
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	//TODO: Think we need to handle more than GET, but that can be added later maybe.
	if(strcasecmp(method, "GET")) 
	{
		clienterror(connfd, pathname, "501", "Not Implemented", "Tiny does not implement this method");
		Pthread_exit(NULL);
	}
	parse_uri(uri, hostname, pathname, &port);
    printf("Thread: %d\n", thread_id);
    // Form HTTP request for origin-server.
	sprintf(client_message, "%s %s %s\r\n", method, pathname, version);
	sprintf(client_message, "%sHost: %s\r\n", client_message, hostname);
	sprintf(client_message, "%s\r\n\r\n", client_message);
	printf("%s", client_message);

	// Open a connection to the server.
	clientfd = Open_clientfd_ts(hostname, port);

	// Initialize file descriptors for both client and server
	Rio_readinitb(&client_rio, clientfd);
	Rio_readinitb(&rio, connfd);
	// Send HTTP request.
	Rio_writen_w(clientfd, client_message, strlen(client_message));

    // Read header, check if chunked.
    while((n = (Rio_readlineb_w(&client_rio, client_buf, MAXLINE))) != 0)
    {
        size += n;
        printf("%s", client_buf);
        Rio_writen_w(connfd, client_buf, n);
        if(strstr(client_buf, "chunked") != NULL)
            chunked = 1;

        if(!strcmp(client_buf, "\r\n"))
            break;
    }
    // If chunked, handle appropriately.
    if(chunked)
    {
        int chunk_size = 0;
        while((n = (Rio_readlineb_w(&client_rio, client_buf, MAXLINE))) != 0)
        {
            //printf("%s", client_buf);
            size += n;
            sscanf(client_buf, "%x", &chunk_size);
            printf("Chunk size: %x\n", chunk_size);
            Rio_writen_w(connfd, client_buf, n);
            
            // If the chunk size is 0 from the start, we know we've reached the end
            // of the chunk.
            if(chunk_size == 0)
                break;

            // Read the rest with Rio_readn, as they're unbuffered bytes
            while(chunk_size > MAXLINE) {
                n = Rio_readn_w(clientfd, client_buf, MAXLINE);
                Rio_writen_w(connfd, client_buf, n);

                size += n;
                chunk_size -= MAXLINE;
            }
            n = Rio_readn_w(clientfd, client_buf, chunk_size);
            size += n;

            Rio_writen_w(connfd, client_buf, n);
        }
        // Read the last empty line of the response.
        /*n = Rio_readlineb_w(&client_rio, client_buf, MAXLINE);
        size += n;
        Rio_writen_w(connfd, client_buf, n);
        */
    }
    else // Read response from the server and write back to client.
    {
        while((n = (Rio_readlineb_w(&client_rio, client_buf, MAXLINE))) != 0)
        {
            size += n;
            Rio_writen_w(connfd, client_buf, n);
        }
    }
	Close(clientfd);
	
    Pthread_mutex_lock(&log_lock);
	
    format_log_entry(logstring, &clientaddr, uri, size);
	FILE *log_entry = Fopen("proxy.log", "a");
	Fwrite(logstring, strlen(logstring), 1, log_entry);
	Fclose(log_entry);

	Pthread_mutex_unlock(&log_lock);

	return NULL;
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;

	if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
	}

	/* Extract the host name */
	hostbegin = uri + 7;
	hostend = strpbrk(hostbegin, " :/\r\n\0");
	len = hostend - hostbegin;
	strncpy(hostname, hostbegin, len);
	hostname[len] = '\0';

	/* Extract the port number */
	*port = 80; /* default */
	if (*hostend == ':')   
	    *port = atoi(hostend + 1);

	/* Extract the path */
	pathbegin = strchr(hostbegin, '/');
	if (pathbegin == NULL) 
	{
		pathname[0] = '/';
		pathname[1] = '\0';
	}
	else 
	{
		pathbegin++;	
		strcpy(pathname, pathbegin - 1);
	}

	return 0;
}

/*
 * clienterror - Sends an HTTP response to the client with an error message.
 *
 * Builds and sends an HTTP response with an appropriate error message if some
 * (obvious) errors come up, this web server lacks a lot of error checking still.
 * Taken from tiny.c from the CS:APP book.
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXBUF];

	// Build the HTTP response
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	// Print the HTTP response
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen_w(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen_w(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen_w(fd, buf, strlen(buf));
	Rio_writen_w(fd, body, strlen(body));
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		char *uri, int size)
{
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;

	/* Get a formatted time string */
	now = time(NULL);
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

	/* 
	 * Convert the IP address in network byte order to dotted decimal
	 * form. Note that we could have used inet_ntoa, but chose not to
	 * because inet_ntoa is a Class 3 thread unsafe function that
	 * returns a pointer to a static variable (Ch 13, CS:APP).
	 */
	host = ntohl(sockaddr->sin_addr.s_addr);
	a = host >> 24;
	b = (host >> 16) & 0xff;
	c = (host >> 8) & 0xff;
	d = host & 0xff;


	/* Return the formatted log entry string */
	sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);
}

/*
 * Rio_readn_w - Modified Rio_readn error wrapper.
 *
 * Instead of calling unix_error on error, return 0 as if encountered EOF.
 */
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes) 
{
	ssize_t n;

	if ((n = rio_readn(fd, ptr, nbytes)) < 0)
		return 0;
	return n;
}

/*
 * Rio_writen_w - Modified Rio_writen error wrapper.
 *
 * Instead of calling unix_error on error, just return.
 */
void Rio_writen_w(int fd, void *usrbuf, size_t n) 
{
	if (rio_writen(fd, usrbuf, n) != n)
		return;
}

/*
 * Rio_readlineb_w - Modified Rio_readlineb error wrapper.
 *
 * Instead of calling unix_error on error, return 0 as if encountered EOF.
 */
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) 
{
	ssize_t rc;

	if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
		return 0;
	return rc;
} 

/*
 * Error wrappers for Pthreads mutex
 */

void Pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
	int rc;

	if((rc = pthread_mutex_init(mutex, attr)) != 0)
		posix_error(rc, "Pthread_mutex_init error");
}

void Pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int rc;

	if((rc = pthread_mutex_lock(mutex)) != 0)
		posix_error(rc, "Pthread_mutex_lock error");
}

void Pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	int rc;

	if((rc = pthread_mutex_unlock(mutex)) != 0)
		posix_error(rc, "Pthread_mutex_unlock error");
}

/*
 * open_clientfd_ts - thread safe version of open_clientfd
 *                      uses lock & copy
 *   open connection to server at <hostname, port> 
 *   and return a socket descriptor ready for reading and writing.
 *   Returns -1 and sets errno on Unix error. 
 *   Returns -2 and sets h_errno on DNS (gethostbyname) error.
 */
/* $begin open_clientfd */
int open_clientfd_ts(char *hostname, int port)
{
	int clientfd;
    // Local hostent p to use for copying.
	struct hostent p, *hp = &p;
    // Shared hostent that receives output from gethostbyname.
    struct hostent *shared_hp;
	struct sockaddr_in serveraddr;

	if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1; /* check errno for cause of error */

	/* Fill in the server's IP address and port */
	Pthread_mutex_lock(&ts_lock);
	if ((shared_hp = gethostbyname(hostname)) == NULL)
		return -2; /* check h_errno for cause of error */

    // Copy contents of shared_hp into local p, hp points to p.
    p = *shared_hp;
	Pthread_mutex_unlock(&ts_lock);

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0], 
			(char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	/* Establish a connection with the server */
	if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
		return -1;
	return clientfd;
}

/******************************************
 * Wrappers for the client/server helper routines 
 ******************************************/
int Open_clientfd_ts(char *hostname, int port) 
{
	int rc;

	if ((rc = open_clientfd_ts(hostname, port)) < 0) {
		if (rc == -1)
			unix_error("Open_clientfd Unix error");
		else        
			dns_error("Open_clientfd DNS error");
	}
	return rc;
}
