#include <ctype.h>
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
#include "mdb.h"
#include "mylist.h"

int main(int argc, char **argv){

    // ignore SIGPIPE so that we don’t terminate when we call
    // send() on a disconnected socket.
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
        perror("signal() failed");
        exit(1);
    }

    if(argc != 3) {
        fprintf(stderr, "usage: %s <database> <server-port>\n", argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[2]);

    // Create a listening socket (also called server socket) 
    int servsock;
    if((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket failed"); 
        exit(1);
    }

    // Construct local address structure
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // any network interface
    servaddr.sin_port = htons(port);

    // Bind to the local address
    if(bind(servsock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
        perror("bind failed"); 
        exit(1);
    }

    // Start listening for incoming connections
    if(listen(servsock, 5 /* queue size for connection requests */ ) < 0){
        perror("listen failed"); 
        exit(1);
    }

    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    while(1){

        clntlen = sizeof(clntaddr); // initialize the in-out parameter
        if((clntsock = accept(servsock,
                        (struct sockaddr *) &clntaddr, &clntlen)) < 0){
            perror("accept failed");
            exit(1);
        }
        char *ip = inet_ntoa(clntaddr.sin_addr);

        fprintf(stderr, "\nconnection started from: ");
        fprintf(stderr, "%s\n", ip);

        FILE *input = fdopen(clntsock, "r");
        if(input == NULL){
            perror("Opening file failed");
            exit(1);
        } 

        // Open database
        struct Node *node;
        struct MdbRec md = { "", "" };
        struct List list;
        initList(&list);
        node = NULL;
        (&list) -> head = node;
        FILE *fp = fopen(argv[1], "rb");
        if(fp == NULL){
            perror("Opening file failed");
            exit(1);
        }
   
        // Data parsed into LinkedList that is allocated in Heap Memory
        while(fread(&md, sizeof(struct MdbRec), 1, fp)){
            struct MdbRec *tM = (struct MdbRec *)malloc(sizeof(struct MdbRec));
            if (tM == NULL){
               perror("malloc failed");
               exit(1);
            }
            *tM = md;
       
            node = addAfter(&list, node, tM);
            if(node == NULL){
                perror("addAfter failed");
                exit(1);
            }
        }
        fclose(fp);    
        

        // mdb-lookup code below
        char in[1024];
        char buffer[1024];
        int charnum;
        while(fgets(in, sizeof(in), input) != NULL){
            if(strlen(in) > 1){
                char str[6];
                if(strlen(in) > 5){
                   strncpy(str, in, 5);
                   str[5] = '\0';
                }
                else{
                   strncpy(str, in, strlen(in));
                   str[strlen(in) - 1] = '\0';
                }
                struct Node *node = (&list)->head;
                int counter = 1;
                while(node){
                    struct MdbRec *tmp = (struct MdbRec *) node -> data;
                    char *n = tmp -> name;
                    char *m = tmp -> msg;
                    if(strstr(n, str) != NULL || strstr(m, str) != NULL){
                        charnum = snprintf(buffer, 1024, "%4d: {%s} said {%s}\n", counter, n, m);
                        if(send(clntsock, buffer, charnum, 0) != charnum){
                            fprintf(stderr, "ERR: send failed\n");
                            break;
                        }
                     }
                     counter++; 
                     node = node -> next;
                }
            }
            else{
                struct Node *node = (&list)->head;
                int counter = 1;
                while(node){
                    struct MdbRec *tmp = node -> data;
                    char *n = tmp -> name;
                    char *m = tmp -> msg;
                    charnum = snprintf(buffer, 1024,"%4d: {%s} said {%s}\n", counter++, n, m);
                    if(send(clntsock, buffer, charnum, 0) != charnum){
                        fprintf(stderr, "ERR: send failed\n");
                        break;
                    }
                    node = node -> next;
                } 
            }
            if(send(clntsock, "\n", 1, 0) != 1){
                fprintf(stderr, "ERR: send failed\n");
                break;
            }
        }
        fprintf(stderr, "connection terminated from: %s\n", ip);

        // frees heap memory
        struct Node *freeN = (&list)->head;
        while(freeN){
            free(freeN -> data);
            freeN = freeN -> next;
        }
        removeAllNodes(&list);
        fclose(input);
    }
}
