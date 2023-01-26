#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUF_SIZE 4096

static void die(const char *s) { perror(s); exit(1); }

int main(int argc, char **argv){

    if(argc != 5) {
        fprintf(stderr, "usage: %s <server_port> <web_root> <mdb-lookup-host> <mdb-lookup-port>\n", argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[1]);
    char *ip = argv[3];
    unsigned short mdbPort = atoi(argv[4]);

    // Creating connection to mdb server
    int mdbSock;
    if ((mdbSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	die("socket failed");

    struct sockaddr_in servaddrs;
    memset(&servaddrs, 0, sizeof(servaddrs)); // must zero out the structure
    servaddrs.sin_family      = AF_INET;
    servaddrs.sin_addr.s_addr = inet_addr(ip);
    servaddrs.sin_port        = htons(mdbPort); // must be in network byte order

    if (connect(mdbSock, (struct sockaddr *) &servaddrs, sizeof(servaddrs)) < 0)
        die("connect failed");

    const char *form =
        "HTTP/1.0 200 OK\r\n\r\n"
        "<h1>mdb-lookup</h1>\n"
        "<p>\n"
        "<form method=GET action=/mdb-lookup>\n"
        "lookup: <input type=text name=key>\n"
        "<input type=submit>\n"
        "</form>\n"
        "<p>\n";

    // Wrapping the TCP connecttion
    FILE *mdb;
    if ((mdb = fdopen(mdbSock, "r")) == NULL) {
	die("fdopen failed");
    }

    // Create a listening socket (also called server socket) 
    int servsock;
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    // Construct local address structure
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // any network interface
    servaddr.sin_port = htons(port);

    // Bind to the local address
    if (bind(servsock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("bind failed");

    // Start listening for incoming connections
    if (listen(servsock, 5 /* queue size for connection requests */ ) < 0)
        die("listen failed");

    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;
    
    // Preparing responses for failure
    char fail[512];
    char fail2[512];
    char fail3[512];
    char fail4[512];
    char aprv[512];
    snprintf(fail, 512,
            "HTTP/1.0 501 Not Implemented\r\n"
            "\r\n"
            "<html><body>\r\n<h1>501 Not Implemented</h1>\r\n</body></html>\r\n");
    snprintf(fail2, 512,
            "HTTP/1.0 400 Bad Request\r\n"
            "\r\n"
            "<html><body>\r\n<h1>400 Bad Request</h1>\r\n</body></html>\r\n");
    snprintf(fail3, 512,
            "HTTP/1.0 403 Forbidden\r\n"
            "\r\n"
            "<html><body>\r\n<h1>403 Forbidden</h1></body>\r\n</html>\r\n");
    snprintf(fail4, 512,
            "HTTP/1.0 404 Not Found\r\n"
            "\r\n"
            "<html><body>\r\n<h1>404 Not Found</h1>\r\n</body></html>\r\n");
    snprintf(aprv, 512, "HTTP/1.0 200 OK\r\n\r\n");
    
    while(1){
        char resp[4096];
        char resp2[4096];
        // Accept an incoming connection
        clntlen = sizeof(clntaddr); // initialize the in-out parameter
        if ((clntsock = accept(servsock,
                        (struct sockaddr *) &clntaddr, &clntlen)) < 0)
            die("accept failed");

        FILE *clnt;
        if((clnt = fdopen(clntsock, "r")) == NULL)
            die("Opening file failed");


        // Read the 1st line
        if(fgets(resp, sizeof(resp), clnt) == NULL) {
	     fclose(clnt);
             continue;

        }
        char *token_separators;
        char *method;
        char *requestURI;
        char *httpVersion; 
        if(strlen(resp) > 2){
            token_separators = "\t \r\n"; // tab, space, new line
            method = strtok(resp, token_separators);
            requestURI = strtok(NULL, token_separators);
            httpVersion = strtok(NULL, token_separators);
        }
        else{
            fprintf(stderr, "%s \"  \" 400 Bad Request\n",
               inet_ntoa(clntaddr.sin_addr));
            send(clntsock, fail2, strlen(fail2), 0);
            fclose(clnt);
            continue;
        }

        // Flush out socket
        for (;;) {
            if (fgets(resp2, sizeof(resp2), clnt) == NULL) {
                fclose(clnt);
                continue;    
            }
            if (strcmp("\r\n", resp2) == 0) {
                break;
            }
        }

        if(strcmp(method, "GET") != 0){
            fprintf(stderr, "%s \"%s %s %s\" 501 Not Implemented\n",
               inet_ntoa(clntaddr.sin_addr), method, 
               requestURI, httpVersion);
            send(clntsock, fail, strlen(fail), 0);
            fclose(clnt);
            continue;
        }
        else if(strcmp(httpVersion, "HTTP/1.1") != 0 && strcmp(httpVersion, "HTTP/1.0") != 0){
            fprintf(stderr, "%s \"%s %s %s\" 2. 501 Not Implemented\n",
               inet_ntoa(clntaddr.sin_addr), method, 
               requestURI, httpVersion);
            send(clntsock, fail, strlen(fail), 0);
            fclose(clnt);
            continue;
        }
        else if(requestURI[0] != '/' || strstr(requestURI, "..")){
            fprintf(stderr, "%s \"%s %s %s\" 400 Bad Request\n",
               inet_ntoa(clntaddr.sin_addr), method, 
               requestURI, httpVersion);
            send(clntsock, fail2, strlen(fail2), 0);
            fclose(clnt);
            continue;
        }
        if(strstr(requestURI, "/mdb-lookup")){
            if(strstr(requestURI, "/mdb-lookup?key=")){
                char key[2056];
                if(strlen(requestURI) > 15){
                    char *p = strrchr(requestURI, '=');
                    if (!p){
                        fprintf(stderr, "%s \"%s %s %s\" 501 Not Implemented\n",
                           inet_ntoa(clntaddr.sin_addr), method, 
                           requestURI, httpVersion);
                        send(clntsock, fail, strlen(fail), 0);
                        fclose(clnt);
                        continue;
                    }
                    snprintf(key, 2056, "%s\n", (p+1));
                    if(send(mdbSock, key, strlen(key), 0) != strlen(key)){
                        fclose(clnt);
                        continue;
                    }
                }
                else{
                   if(send(mdbSock, "\n", strlen("\n"), 0) != strlen("\n")){
                      fclose(clnt);
                      continue;
                   }
                }
                size_t last = strlen(key) - 1;
                if (key[last] == '\n')
                    key[last] = '\0';

                char response[2056];
                snprintf(response, 2056, "%s<p><table border>\r\n", form);
                if(send(clntsock, response,
                            strlen(response), 0) != strlen(response)){
                    fclose(clnt);
                    continue;
                }
                fprintf(stderr, "looking up [%s]: %s \"%s %s %s\" 200 OK\n",
                   key, inet_ntoa(clntaddr.sin_addr), method, 
                   requestURI, httpVersion);
                char buffer[BUF_SIZE];
                char final[BUF_SIZE + 25]; 
                
                int even = 0;
                while(fgets(buffer, sizeof(buffer), mdb) != NULL){
                    if(strcmp(buffer,"\n") != 0){
                        if(even != 1){
                            even = 1;
                            snprintf(final, BUF_SIZE + 25,
                                    "<tr><td>%s\r\n", buffer);
                            if(send(clntsock, final, strlen(final), 0) != strlen(final)){
                                fclose(clnt);
                                continue;
                            }
                        }
                        else{
                            even = 0;
                            snprintf(final, BUF_SIZE + 25,
                                    "<tr><td bgcolor = yellow>%s\r\n", buffer);
                            if(send(clntsock, final, strlen(final), 0) != strlen(final)){
                                fclose(clnt);
                                continue;
                            }
                        }
                    }      
                    else{
                        break;
                    }
                }
                char *end = "</table>\r\n</body></html>\r\n";
                if(send(clntsock, end, strlen(end), 0) != strlen(end)){
                    fclose(clnt);
                    continue;
                }
            }
            else{
                fprintf(stderr, "%s \"%s %s %s\" 200 OK\n",
                   inet_ntoa(clntaddr.sin_addr), method, 
                   requestURI, httpVersion);
                if(send(clntsock, form, strlen(form), 0) != strlen(form)){
                    fclose(clnt);
                    continue;
                }
            }
        }  
        else{
            // Check if Directory & Error Handling 
            char filepath[2056];
            char temp[2056];
            snprintf(temp, 2056, "%s%s", argv[2], requestURI);
            struct stat st;
            if(stat(temp, &st) != 0){
                fprintf(stderr, "%s \"%s %s %s\" This - 404 Not Found\n",
                   inet_ntoa(clntaddr.sin_addr), method, 
                   requestURI, httpVersion);
                send(clntsock, fail4, strlen(fail4),0);
                fclose(clnt);
                continue;
            }
            if(S_ISDIR(st.st_mode)){
                if(temp[strlen(temp)-1] != '/'){
                    fprintf(stderr, "%s \"%s %s %s\" 403 Forbidden\n",
                       inet_ntoa(clntaddr.sin_addr), method, 
                       requestURI, httpVersion);
                    send(clntsock, fail3, strlen(fail3),0);
                    fclose(clnt);
                    continue;
                }
                else{
                    snprintf(filepath, 2056, "%sindex.html", temp);
                }
            }
            else{
                snprintf(filepath, 2056, "%s", temp);
            }
            
            // Open File
            FILE *fd;
            fd = fopen(filepath, "rb");
            if(fd == NULL){
                fprintf(stderr, "%s \"%s %s %s\" 404 Not Found\n",
                   inet_ntoa(clntaddr.sin_addr), method, 
                   requestURI, httpVersion);
                send(clntsock, fail4, strlen(fail4), 0);
                fclose(clnt);
                continue;
            }
            else{
                fprintf(stderr, "%s \"%s %s %s\" 200 OK\n",
                   inet_ntoa(clntaddr.sin_addr), method, 
                   requestURI, httpVersion);
                if(send(clntsock, aprv, strlen(aprv), 0) != strlen(aprv)){
                    fclose(fd);
                    fclose(clnt);
                    continue;
                }
            }

            // Sending data
            char buf[BUF_SIZE];
            size_t n;
            while((n = fread(buf, sizeof(char), sizeof(buf), fd)) > 0){
                if(send(clntsock, buf, n, 0) != n){
                    fclose(fd);
                    fclose(clnt);
                    continue;
                }
            }
            if(ferror(fd)){
                fclose(fd);
                fclose(clnt);
                continue;
            }
            fclose(fd);
        }
        fclose(clnt);
    }
    fclose(mdb);
    return 0;
}
        
