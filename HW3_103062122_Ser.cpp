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
#include <map>
#include <vector>
#define MAX 2048

using namespace std;

pthread_mutex_t userAndPassword_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t onlineUserList_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fdToUsername_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t userFileList_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileUserList_mutex = PTHREAD_MUTEX_INITIALIZER;

map<string, vector<string> > fileUserList;
map<string, vector<string> > userFileList;
map<string, string> userAndPassword;
vector<string> onlineUserList;
string fdToUsername[FD_SETSIZE];

inline void lockUserInf() {
	pthread_mutex_lock(&userAndPassword_mutex);
	pthread_mutex_lock(&onlineUserList_mutex);
	pthread_mutex_lock(&fdToUsername_mutex);
	pthread_mutex_lock(&userFileList_mutex);
	pthread_mutex_lock(&fileUserList_mutex);
}

inline void unlockUserInf() {
	pthread_mutex_unlock(&userAndPassword_mutex);
	pthread_mutex_unlock(&onlineUserList_mutex);
	pthread_mutex_unlock(&fdToUsername_mutex);
	pthread_mutex_unlock(&userFileList_mutex);
	pthread_mutex_unlock(&fileUserList_mutex);
}

void readFileList(int sockfd, char *input) {
	lockUserInf();
	string userName = fdToUsername[sockfd];
	userFileList[userName].clear();
	char *token = strtok(input, " ");
	token = strtok(NULL, " ");
	while (token) {
		string dataName = token;
		userFileList[userName].push_back(dataName);
		token = strtok(NULL, " ");
	}
	printf("User %s has:\n", userName.data());
	for (unsigned i = 0; i < userFileList[userName].size(); i++) {
		puts(userFileList[userName][i].data());
	}
	unlockUserInf();
}

void deleteAccount(int sockfd) {
	lockUserInf();
	char sendline[MAX] = {0};
	string userName = fdToUsername[sockfd];
	userAndPassword[userName] = "";
	puts("A user just deleted his/her account.");
	sprintf(sendline, "User %s is deleted, logged out.\n", userName.data());
	write(sockfd, sendline, strlen(sendline));
	unlockUserInf();
}

void registerAccount(int sockfd, char *username, char *password) {
	lockUserInf();
	char sendline[MAX] = {0};
	string userName = username;
	string passWord = password;
	if (userAndPassword[userName] == "") {
		userAndPassword[userName] = passWord;
		onlineUserList.push_back(userName);
		fdToUsername[sockfd] = userName;
		puts("A user just registered.");
		sprintf(sendline, "ok");
		write(sockfd, sendline, strlen(sendline));
	} else {
		sprintf(sendline, "no");
		write(sockfd, sendline, strlen(sendline));
	}
	unlockUserInf();
}

void loginAccount(int sockfd, char *username, char *password) {
	lockUserInf();
	char sendline[MAX] = {0};
	string userName = username;
	string passWord = password;
	if (userAndPassword[userName] == "") {
		puts("Somebody entered wrong username or password.");
		sprintf(sendline, "no");
		write(sockfd, sendline, strlen(sendline));
	} else {
		onlineUserList.push_back(userName);
		fdToUsername[sockfd] = userName;
		puts("A user just login.");
		sprintf(sendline, "ok");
		write(sockfd, sendline, strlen(sendline));
	}
	unlockUserInf();
}

void *run(void *arg) {
	char recv[MAX] = {0};
	int connfd;
	connfd = *((int *) arg);
	pthread_detach(pthread_self());
	while (read(connfd, recv, MAX) > 0) {
		char command[MAX] = {0};
		sscanf(recv, "%s", command);
		if (!strcmp("R", command)) {
			char username[100] = {0}, password[100] = {0};
			sscanf(recv, "%*s%s%s", username, password);
			registerAccount(connfd, username, password);
		} else if (!strcmp("L", command)) {
			char username[100] = {0}, password[100] = {0};
			sscanf(recv, "%*s%s%s", username, password);
			loginAccount(connfd, username, password);
		} else if (!strcmp("D", command)) {
			deleteAccount(connfd);
		} else if (!strcmp("FileList", command)) {
			readFileList(connfd, recv);
		}
		bzero(recv, sizeof(recv));
	}
	lockUserInf();
	unsigned idxToDelete;
	for (idxToDelete = 0; idxToDelete < onlineUserList.size(); idxToDelete++)
		if (onlineUserList[idxToDelete] == fdToUsername[connfd]) {
			onlineUserList.erase(onlineUserList.begin() + idxToDelete);
			break;
		}
	fdToUsername[connfd] = "";
	unlockUserInf();
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

	if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		puts("Bind error.");
		exit(0);
	}
	if (listen(listenfd, 1024) < 0) {
		puts("Listen error.");
		exit(0);
	}

	while (1) {
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
		printf("Connection from: %s, port: %d.\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		pthread_create(&tid, NULL, &run, (void *) &connfd);
	}

	return 0;
}
