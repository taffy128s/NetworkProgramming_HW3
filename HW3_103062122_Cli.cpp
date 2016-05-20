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

void showMenu() {

}

int main(int argc, char **argv) {
	if (argc != 3) {
		puts("Usage: a.out <ServerIP> <ServerPort>");
		exit(0);
	}

	char sendline[MAX], command[MAX], recv[MAX];
	int sockfd;
	struct sockaddr_in servaddr, sin;
	socklen_t len = sizeof(sin);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		puts("Connect error.");
		exit(0);
	}

	if (getsockname(sockfd, (struct sockaddr *) &sin, &len) < 0) {
		puts("Getsockname error.");
	} else {
		printf("Port number: %d\n", ntohs(sin.sin_port));
	}

	puts("**********Welcome**********");
	puts("[R]egister [L]ogin");
	bzero(sendline, sizeof(sendline));
	fgets(sendline, MAX, stdin);
	if (!strcmp("R\n", sendline)) {
		puts("Please enter a new pair of username and password.");
		bzero(command, sizeof(command));
		fgets(command, MAX, stdin);
		strcat(sendline, command);
		write(sockfd, sendline, strlen(sendline));
		bzero(recv, sizeof(recv));
		read(sockfd, recv, MAX);
		if (!strcmp("ok", recv)) {
			puts("Registered successfully.");
		} else {
			puts("Username is used.");
			exit(0);
		}
	} else if (!strcmp("L\n", sendline)) {
		puts("Please enter an existing pair of username and password.");
		bzero(command, sizeof(command));
		fgets(command, MAX, stdin);
		strcat(sendline, command);
		write(sockfd, sendline, strlen(sendline));
		bzero(recv, sizeof(recv));
		read(sockfd, recv, MAX);
		if (!strcmp("ok", recv)) {
			puts("Login successfully.");
		} else {
			puts("Failed to login.");
			exit(0);
		}
	} else {
		puts("You didn't input a valid command.");
		exit(0);
	}
	while (1) {
		puts("You can input the following commands:");
		showMenu();
		puts("Your input:");
		bzero(sendline, sizeof(sendline));
		bzero(command, sizeof(command));
		bzero(recv, sizeof(recv));
		if (fgets(sendline, MAX, stdin) == NULL) break;

	}

	return 0;
}
