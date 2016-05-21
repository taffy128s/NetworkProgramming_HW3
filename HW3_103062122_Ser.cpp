#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
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
#include <set>
#define MAX 2048

pthread_mutex_t userAndPassword_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t onlineUserList_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fdToUsername_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t userFileList_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileUserList_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileSet_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fdToCliaddr_mutex = PTHREAD_MUTEX_INITIALIZER;

std::set<std::string> fileSet;
std::map<std::string, std::vector<std::string> > fileUserList;
std::map<std::string, std::vector<std::string> > userFileList;
std::map<std::string, std::string> userAndPassword;
std::vector<std::string> onlineUserList;
std::string fdToUsername[FD_SETSIZE];
struct sockaddr_in fdToCliaddr[FD_SETSIZE];

inline void lockUserInf() {
	pthread_mutex_lock(&userAndPassword_mutex);
	pthread_mutex_lock(&onlineUserList_mutex);
	pthread_mutex_lock(&fdToUsername_mutex);
	pthread_mutex_lock(&userFileList_mutex);
	pthread_mutex_lock(&fileUserList_mutex);
	pthread_mutex_lock(&fileSet_mutex);
	pthread_mutex_lock(&fdToCliaddr_mutex);
}

inline void unlockUserInf() {
	pthread_mutex_unlock(&userAndPassword_mutex);
	pthread_mutex_unlock(&onlineUserList_mutex);
	pthread_mutex_unlock(&fdToUsername_mutex);
	pthread_mutex_unlock(&userFileList_mutex);
	pthread_mutex_unlock(&fileUserList_mutex);
	pthread_mutex_unlock(&fileSet_mutex);
	pthread_mutex_unlock(&fdToCliaddr_mutex);
}

inline int findUserFD(std::string userName) {
	for (int i = 0; i < FD_SETSIZE; i++) {
		if (fdToUsername[i] == userName) return i;
	}
	return INT_MAX;
}

void sendUserIPPort(int sockfd, char *username) {
	lockUserInf();
	std::string targetUser = username;
	int targetFD = findUserFD(targetUser);
	struct sockaddr_in sin = fdToCliaddr[targetFD];
	char sendline[MAX] = {0};
	char port[10] = {0};
	sprintf(port, " %d", ntohs(sin.sin_port));
	sprintf(sendline, "%s ", username);
	strcat(sendline, inet_ntoa(sin.sin_addr));
	strcat(sendline, port);
	write(sockfd, sendline, strlen(sendline));
	unlockUserInf();
}

void sendFileList(int sockfd) {
	lockUserInf();
	char sendline[MAX] = {0};
	sprintf(sendline, "Files on server are:");
	for (auto file : fileSet) {
		strcat(sendline, "\n");
		strcat(sendline, file.data());
	}
	strcat(sendline, "\n");
	write(sockfd, sendline, strlen(sendline));
	unlockUserInf();
}

void sendUserList(int sockfd) {
	lockUserInf();
	char sendline[MAX] = {0};
	sprintf(sendline, "Online users are:");
	for (auto user : onlineUserList) {
		strcat(sendline, "\n");
		strcat(sendline, user.data());
	}
	strcat(sendline, "\n");
	write(sockfd, sendline, strlen(sendline));
	unlockUserInf();
}

void mergeFileList() {
	lockUserInf();
	fileSet.clear();
	fileUserList.clear();
	for (auto user : onlineUserList) {
		for (auto file : userFileList[user]) {
			fileSet.insert(file);
			fileUserList[file].push_back(user);
		}
	}
	unlockUserInf();
}

void readFileList(int sockfd, char *input) {
	lockUserInf();
	std::string userName = fdToUsername[sockfd];
	userFileList[userName].clear();
	char *token = strtok(input, " ");
	token = strtok(NULL, " ");
	while (token) {
		std::string dataName = token;
		userFileList[userName].push_back(dataName);
		token = strtok(NULL, " ");
	}
	puts("---------------");
	printf("User %s has:\n", userName.data());
	for (unsigned i = 0; i < userFileList[userName].size(); i++) {
		puts(userFileList[userName][i].data());
	}
	puts("---------------");
	unlockUserInf();
}

inline void removeOnlineStatus(int sockfd) {
	lockUserInf();
	unsigned idxToDelete;
	for (idxToDelete = 0; idxToDelete < onlineUserList.size(); idxToDelete++)
		if (onlineUserList[idxToDelete] == fdToUsername[sockfd]) {
			onlineUserList.erase(onlineUserList.begin() + idxToDelete);
			break;
		}
	fdToUsername[sockfd] = "";
	unlockUserInf();
}

void deleteAccount(int sockfd) {
	lockUserInf();
	char sendline[MAX] = {0};
	std::string userName = fdToUsername[sockfd];
	userAndPassword[userName] = "";
	puts("A user just deleted his/her account.");
	sprintf(sendline, "User %s is deleted, logged out.\n", userName.data());
	write(sockfd, sendline, strlen(sendline));
	unlockUserInf();
}

void registerAccount(int sockfd, char *username, char *password) {
	lockUserInf();
	char sendline[MAX] = {0};
	std::string userName = username;
	std::string passWord = password;
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
	std::string userName = username;
	std::string passWord = password;
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
			mergeFileList();
		} else if (!strcmp("SU", command)) {
			sendUserList(connfd);
		} else if (!strcmp("SF", command)) {
			sendFileList(connfd);
		} else if (!strcmp("T", command)) {
			char username[100] = {0};
			sscanf(recv, "%*s%s", username);
			sendUserIPPort(connfd, username);
		}
		bzero(recv, sizeof(recv));
	}
	removeOnlineStatus(connfd);
	mergeFileList();
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
		lockUserInf();
		fdToCliaddr[connfd] = cliaddr;
		unlockUserInf();
		printf("Connection from: %s, port: %d.\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		pthread_create(&tid, NULL, &run, (void *) &connfd);
	}

	return 0;
}
