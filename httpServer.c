#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define BUFSIZE 8192
#define LISTENQ 1024 

int startListen(int port);
void handleClient(int clientSock);
void handleError(int errNum, int clientSock);
void handleReq(int clientSock, char *fName, char *version);
int handleType(char *buf, char *type);

int main(int argc, char **argv){
    int portno = atoi(argv[1]); /* port to listen on */
    struct sockaddr_in clientaddr; /* client addr */
    int clientlen; /* byte size of client's address */
    int sockfd; /* socket */
    int *clientSock; /*client request*/

    /*check command line arguments*/
    if (argc != 2){
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sockfd = startListen(portno);

    while (1){
        /*Accept connection*/
        clientSock = malloc(sizeof(int));
        *clientSock = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (clientSock < 0){
            perror("Accept failure");
            exit(EXIT_FAILURE);
        }
        handleClient(clientSock);
    }
}

void handleError(int errNum, int clientSock){
    char msg[BUFSIZE];
    switch (errNum){
        case 400:
            sprintf(msg, "400 Bad Request");
            write(clientSock, msg, strlen(msg));
            break;
        case 403:
            sprintf(msg, "403 Forbidden");
            write(clientSock, msg, strlen(msg));
            break;
        case 404:
            sprintf(msg, "404 Not Found");
            write(clientSock, msg, strlen(msg));
            break;
        case 405:
            sprintf(msg, "405 Method Not Allowed");
            write(clientSock, msg, strlen(msg));
            break;
        case 505:
            sprintf(msg, "505 HTTP Version Not Supported");
            write(clientSock, msg, strlen(msg));
            break;
    }
    return; 
}

int handleType(char *fName, char *ext){
    if(strstr(fName, ".html") != NULL){
        strcpy(ext, "text/html");
        return 0;
    }
    if(strstr(fName, ".txt") != NULL){
        strcpy(ext, "text/plain");
        return 0;
    }
    if(strstr(fName, ".png") != NULL){
        strcpy(ext, "image/png");
        return 0;
    }
    if(strstr(fName, ".gif") != NULL){
        strcpy(ext, "image/gif");
        return 0;
    }
    if(strstr(fName, ".jpg") != NULL){
        strcpy(ext, "image/jpg");
        return 0;
    }
    if(strstr(fName, ".ico") != NULL){
        strcpy(ext, "image/x-icon");
        return 0;
    }
    if(strstr(fName, ".css") != NULL){
        strcpy(ext, "text/css");
        return 0;
    }
    if(strstr(fName, ".js") != NULL){
        strcpy(ext, "application/javascript");
        return 0;
    }
    return -1;
}

void handleReq(int clientSock, char *fName, char *version){
    FILE *f;
    char buf[BUFSIZE]; /*Hold header*/
    char ext[BUFSIZE];/*Hold response from handleType()*/
    char path[BUFSIZE]; /*Hold path to file*/
    int errNum; /*error code number*/
    size_t size; /*file size*/

    strcat(path, "www/");
    strcat(path, fName);
    f = fopen(path, "rb");
    if(f == NULL){
        errNum = 404;
        handleError(errNum, clientSock);
        return;
    }

    /*Calculate file size*/
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /*Get content type*/
    if(handleType(fName, ext) != 0){
        errNum = 400;
        handleError(errNum, clientSock);
        return;
    }

    /*Send header*/
    char header[BUFSIZE];
    sprintf(header, "%s 200 OK\r\nContent-Type:%s\r\nContent-Length:%ld\r\n", version, ext, size);
    strcpy(buf, header);
    write(clientSock, buf, strlen(header));

    /*Send file content*/
    int n;
    while(n = fread(path, 1, BUFSIZE, f), n > 0){
        if(n = write(clientSock, path, n), n < 0){
            errNum = 403;
            handleError(errNum, clientSock);
            return;
        }
    }
    fclose(f);
}

void handleClient(int clientSock){
    char buf[BUFSIZE]; /*message buf*/
    int n; /*message byte size*/
    char *reqType; /*type of request*/
    char *fName; /*file name/path*/
    char *version; /*HTTP verison*/
    char *ptr; /*pointer in request string*/
    int errNum; /*which error to throw*/

    /*Receive request*/
    n = read(clientSock, buf, BUFSIZE);
    
    /*Split request*/
    reqType = strtok_r(buf, "/", &ptr);/*Get request type (GET or PUT)*/
    if(reqType == NULL || strcmp(reqType, "GET") != 0){
        errNum = 405;
        handleError(errNum, clientSock);
        return;
    }
    fName = strtok_r(NULL, "/", &ptr); /*Get file lockation/path*/
    version = strtok(NULL, "\r"); /*Get version*/
    if(version == NULL || strcmp(version, "HTTP/1.1") != 0 || strcmp(version, "HTTP/1.0") != 0){
        errNum = 400;
        handleError(errNum, clientSock);
        return;
    }
    
    handleReq(clientSock, fName, version);
}

int startListen(int port){
    int sockfd; /* socket */
    struct sockaddr_in serveraddr; /* server's addr */
    int optval; /* flag value for setsockopt */

    /*create parent socket*/
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        perror("ERROR opening socket");
        return -1;
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
    /*Build server's internet address*/
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    /*bind server socket*/
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
        perror("ERROR on binding");
        return -1;
    /*Listen for incoming connections*/
    if (listen(sockfd, LISTENQ) < 0){
        perror("Listen failed");
        close(sockfd);
        return -1;
    }
    return sockfd;
}