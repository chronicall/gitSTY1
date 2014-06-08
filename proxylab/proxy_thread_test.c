/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Sandra Ros Hrefnu Jonsdottir, sandrarj13@ru.is
 *     Grettir Olafsson, grettir10@ru.is
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
void echo(int connfd);
void doit(int connfd);
int parse_uri(char *uri, char *hostname, char *pathname);//, int  *port);
void read_requesthdrs(rio_t *rp);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
int open_clientfd_ts(char *hostname, int port);
void *thread(void *vargp);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp;
    pthread_t tid;

    /* Check arguments */
    if (argc != 2) {
	    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	    exit(0);
    }
    port = atoi(argv[1]);
    
    listenfd = Open_listenfd(port); // Start listening on the port supplied.
    while(1)
    {
     
        clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (struct sockaddr*)&clientaddr, (socklen_t*)&clientlen);

        // Determine the domain name and IP address of the client
        hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
                           sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("server connected to %s (%s)\n", hp->h_name, haddrp);

        Pthread_create(&tid, NULL, thread, (void *) connfd);
    }
    exit(0);
}

/*
 * echo - Echo the client message
 */
void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}

/*
 * doit - Handle HTTP requests
 */
void doit(int connfd)
{
    int clientfd, port = 80;
    size_t n, size = 0;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE];
    char client_buf[MAXLINE], client_message[MAXBUF];
    //char logstring[MAXLINE];
    //
    rio_t rio, client_rio;
    //struct hostent *p;

    // Read request line and headers
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET")) 
    {
        clienterror(connfd, pathname, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }
    parse_uri(uri, hostname, pathname);// port);
    sprintf(client_message, "%s %s %s\r\n", method, pathname, version);
    
    /*while(strcmp(buf, "\r\n"))
    {
        Rio_readlineb(&rio, buf, MAXLINE);
        sprintf(client_message, "%s%s", client_message, buf);
    }*/

    sprintf(client_message, "%sHost: %s\r\n", client_message, hostname);
    sprintf(client_message, "%s\r\n\r\n", client_message);
    printf("%s", client_message);
    
    // Open a connection to the server.
    clientfd = Open_clientfd(hostname, port);

    // Initialize file descriptors for both client and server
    Rio_readinitb(&client_rio, clientfd);
    Rio_readinitb(&rio, connfd);
    // Send HTTP request.
    Rio_writen(clientfd, client_message, strlen(client_message));
    int counter = 0;
    // Read the response and write it out, write back to client. (at least it should..)
    while((n = (Rio_readlineb(&client_rio, client_buf, MAXLINE))) != 0)
    {
        counter++;
        size += n;
        printf("%s", client_buf);
        Rio_writen(connfd, client_buf, n);
    }
    /*format_log_entry(logstring, clientaddr, uri, size);
    FILE *log_entry = Fopen("proxy.log", "a+");
    Fwrite(logstring, strlen(logstring), 1, log_entry);
    Fclose(log_entry);
	*/
}

/*
 * read_requesthdr - Read the HTTP request header from client
 */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n"))
    {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname)//, int *port)
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
    //*port = 80; /* default */
    //if (*hostend == ':')   
	    //*port = atoi(hostend + 1);
    
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
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
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
void *thread(void *vargp)
{
 	int connfd = *((int *)vargp);
	pthread_detach(pthread_self());
	free(vargp);
	doit(connfd);
	close(connfd);
	return NULL;
}

int open_clientfd_ts(char *hostname, int port)
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
    if ((hp = gethostbyname(hostname)) == NULL)
	return -2; /* check h_errno for cause of error */
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
