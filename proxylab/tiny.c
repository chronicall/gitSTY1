/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Sandra Ros Hrefnu Jonsdottir, sandrarj13@ru.is
 *     Grettir Olafsson, grettir10@ru.is
 * 
 * An exact copy of the tiny web server given in the CS:APP textbook.
 * A starting point for something great!
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *hostname, char *pathname);//, int *port); //char *cgiargs)
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
//void serve_dynamic(int fd, char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
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
    if (argc != 2) 
    {
	    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	    exit(0);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);

    while(1) 
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // Determine the domain name and IP address of the client
        hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                            sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("server connected to %s (%s)\n", hp->h_name, haddrp);
        
        doit(connfd);
        Close(connfd);
    }

    exit(0);
}

/*
 * doit - Handles one HTTP request
 *
 * After some minor error checking, check whether to server static or dynamic
 * content.
 */
void doit(int fd)
{
    int is_static;//, *port;
    struct stat sbuf;
    //size_t n;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE];//cgiargs[MAXLINE];filename[MAXLINE],
    rio_t rio;

    // Read request line and headers
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET")) 
    {
        clienterror(fd, pathname, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    // Parse URI from GET request
    is_static = parse_uri(uri, hostname, pathname);//, port);
    printf("%s\r\n", hostname);
    printf("%s\r\n", pathname);
    if(stat(pathname, &sbuf) < 0) 
    {
        clienterror(fd, pathname, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    if(!is_static) // Server Static content
    {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            clienterror(fd, pathname, "403", "Forbidden", "Tiny couldn]t read this file");
            return;
        }
        serve_static(fd, pathname, sbuf.st_size);
    }
    /*else // Serve Dynamic content
    {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn]t read this file");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }*/

    /*while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("server received %d bytes\n", n);
        Rio_writen(connfd, buf, n);
    }*/
}

/*
 * read_requesthdrs - Read the header of the HTTP request
 *
 * Just reads it and prints it out to the console. So far. Can use this to check headers, 
 * check for dates for updating, etc.
 */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
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
 * Tiny: Assume the content for static content is the current directory
 * and home directory for executeable is ./cgi-bin. Any URI containing
 * cgi-bin is assumed to to have a request for dynamic content. Default
 * file name is ./home.html
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname)//, int *port) //char *cgiargs)
{
    /*char *ptr;

    if(!strstr(uri, "cgi-bin")) // Static content
    {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if(uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html");
        return 1;
    }
    else // Dynamic content
    {
        ptr = index(uri, '?');
        if(ptr) // If there are any arguments
        {
            strcpy(cgiargs, ptr + 1); // Copy arguments to cgiargs
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");

        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }*/

    // From original proxy.c file, from the handout
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
       
    // Extract the host name
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    // Extract the port number
    //*port = 80; // default
    //if (*hostend == ':')   
	//*port = atoi(hostend + 1);
    
    // Extract the path
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
 * serve_static - Serve Static content
 *
 * Tiny serves four types of static content, HTML files, unformatted text files
 * and images encoded in GIF and JPET format.
 */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    // Send response headers to client
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));

    // Send response body to client
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
 * get_filetype - Get filetype from file
 */
void get_filetype(char *filename, char *filetype)
{
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if(strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

/*
 * serve_dynamic - Serve dynamic content
 *
 * Fork a child process and run the CGI program in the context of the child.
 */
/*void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    // Return first part of the HTTP response
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if(Fork() == 0)
    {
        // Real server would set all CGI vars here
        setenv("QUERY_STRING", cgiargs, 1);
        // Redirecte stdout to client
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    // Wait for child to terminate and reap it.
    Wait(NULL);
}*/

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
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


