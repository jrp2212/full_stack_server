#include <stdio.h>
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

static void die(const char *s) { perror(s); exit(1); }

int main(int argc, char **argv){
    if(argc != 4) {
        fprintf(stderr, "usage: %s <server> <server-port> <url-path>\n",
                argv[0]);
        exit(1);
    }

    struct hostent *he;
    char *serverName = argv[1];

    // Get server ip from server name
    if((he = gethostbyname(serverName)) == NULL) {
        die("gethostbyname failed");
    }
    const char *ip = inet_ntoa(*(struct in_addr *)he->h_addr);
    unsigned short port = atoi(argv[2]);


    // Create a socket for TCP connection
    int sock; // socket descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");
    FILE *input = fdopen(sock, "rb");
    if(input == NULL)
        die("Opening file failed");


    // Construct a server address structure
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr)); // must zero out the structure
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port        = htons(port); // must be in network byte order


    // Establish a TCP connection to the server
    if (connect(sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("connect failed");
   

    // Formatting and sending GET request 
    char get[1024];
    snprintf(get, 1024, "GET %s HTTP/1.0\r\nHost:%s:%s\r\n\r\n", argv[3], argv[1], argv[2]);
    int len = strlen(get);
    if(send(sock, get, len, 0) != len)
        die("send failed");

    // Reading HTTP Response (part1)
    char resp[1024];
    int check = 0;
    while(fgets(resp, sizeof(resp), input) != NULL){
        if(check == 0 && !(strstr(resp, "200 OK"))){
            perror(resp);
            exit(1);
        }
        check = 1;
        if(resp[0] == '\r' && resp[1] == '\n'){
            break;
        }
    }

    // Reading HTTP Response (part2)
    char *filename = strrchr(argv[3], '/');
    filename = filename+1; 
    FILE *fp = fopen(filename, "wb");
    if(fp == NULL)
        die("Opening file failed");
    
    char cont[50];
    int bytes = 0;

    while((bytes = fread(cont, 1, sizeof(cont), input)) > 0){
        fwrite(cont, 1, bytes, fp);
    }
      
    fclose(input);
    fclose(fp);
    return 0;
}
