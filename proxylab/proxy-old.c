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
int parse_uri(char *uri, char *hostname, char *pathname);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void send_connect(char *hostname, char *uri, char *pathname);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp;

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
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);

        // Determine the domain name and IP address of the client
        hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
                           sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("server connected to %s (%s)\n", hp->h_name, haddrp);

        echo(connfd);
        Close(connfd);
    }
    exit(0);
}

/*
 * echo - Echo the client message
 */
void echo(int connfd)
{
    char buf[MAXLINE], uri[MAXLINE], method[MAXLINE], version[MAXLINE],  hostname[MAXLINE], pathname[MAXLINE];
    int is_static;
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET"))
	{
		printf("501, %s is not implemented", method);
		return;
	}
    else
	{
		is_static = parse_uri(uri, hostname, pathname);
		send_connect(hostname, uri, pathname);
	}
}

void send_connect(char *hostname, char *uri, char *pathname)
{
	char request[1000];
	struct sockaddr_in serveraddr;
	struct hostent *server;
	int port = 80;
	
	int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(tcpSocket < 0)
		printf("Error opening socket\n");
	else
		printf("Successfully opened socket\n");
	server = gethostbyname(hostname);
	
	if(server == NULL)
	{
		printf("Host was empty\n");
	}
	
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);

	serveraddr.sin_port = htons(port);

	if(connect(tcpSocket, (struct sockaddr*) &serveraddr, sizeof(serveraddr))<0)
		printf("Error connecting\n");
	else
		printf("Connect succesful\n");
	
	bzero(request, 1000);
	
	sprintf(request, "GET %s HTTP/1.1\r\n Host: %s\r\n\r\n", uri, pathname);

	if(send(tcpSocket, request, strlen(request), 0) < 0)
		printf("Error with send()\n");
	else
		printf("succesfully sent html fetch request\n");

	bzero(request, 1000);
	
	int iResult;
	do {
		iResult = recv(tcpSocket, request, 999, 0);
		if( iResult > 0)
			printf("Bytes received: %d\n", iResult);
		else if(iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error:\n");
	}while( iResult > 0);
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname)
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
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
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
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


