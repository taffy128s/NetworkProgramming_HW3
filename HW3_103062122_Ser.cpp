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

void *run(void *arg) {
	char recv[MAX] = {0};
	int connfd;
	connfd = *((int *) arg);
	pthread_detach(pthread_self());
	while (read(connfd, recv, MAX) > 0) {
		
		bzero(recv, sizeof(recv));
	}
	puts("A thread terminated.");
	close(connfd);
	return NULL;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		puts("Usage: a.out <Port>");
		exit(0);
	}
	
	const int on = 1;
	int listenfd, connfd;
	socklen_t clilen;
	pthread_t tid;
	struct sockaddr_in servaddr, cliaddr;
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));
	
	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	listen(listenfd, 1024);
	
	while (1) {
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
		printf("Connection from: %s, port: %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		pthread_create(&tid, NULL, &run, (void *) &connfd);
	}
	return 0;
}