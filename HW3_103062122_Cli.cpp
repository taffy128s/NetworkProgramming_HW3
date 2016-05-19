#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <string>
#include <sstream>
#define MAX 2048

int main(int argc, char **argv) {
    if (argc != 3) {
        puts("Usage: a.out <ServerIP> <ServerPort>");
        exit(0);
    }
    
    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    
    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    
    while (1) {
        puts("You can input the following commands:");
        break;
    }
    
    return 0;
}