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

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

std::set<std::string> fileSet;
std::map<std::string, int> fileSizeMap;
std::map<std::string, std::vector<std::string> > fileUserList;
std::map<std::string, std::vector<std::string> > userFileList;
std::map<std::string, std::string> userAndPassword;
std::vector<std::string> onlineUserList;
std::string fdToUsername[FD_SETSIZE];
struct sockaddr_in fdToCliaddr[FD_SETSIZE];

inline int findUserFD(std::string userName) {
	for (int i = 0; i < FD_SETSIZE; i++) {
		if (fdToUsername[i] == userName) return i;
	}
	return INT_MAX;
}

void stopDownload(int sockfd, char *filename) {
	pthread_mutex_lock(&mutex);
	std::string fileName = filename;
	std::vector<std::string> &list = fileUserList[fileName];
	int ownerNum = fileUserList[fileName].size();
	for (int i = 0; i < ownerNum; i++) {
		int udpfd;
		struct sockaddr_in udpaddr;
		socklen_t len;
		len = sizeof(udpaddr);
		bzero(&udpaddr, sizeof(udpaddr));
		udpaddr.sin_family = AF_INET;
		int sourceFD = findUserFD(list[i]);
		udpaddr.sin_port = fdToCliaddr[sourceFD].sin_port;
		udpaddr.sin_addr = fdToCliaddr[sourceFD].sin_addr;
		udpfd = socket(AF_INET, SOCK_DGRAM, 0);
		char sendline[MAX] = {0};
		sprintf(sendline, "stop\n");
		sendto(udpfd, sendline, strlen(sendline), 0, (struct sockaddr *) &udpaddr, len);
		close(udpfd);
	}
	pthread_mutex_unlock(&mutex);
}

void initialDownload(int sockfd, char *filename) {
	pthread_mutex_lock(&mutex);
	std::string fileName = filename;
	char sendline[MAX] = {0};
	if (fileSet.find(fileName) == fileSet.end()) {
		sprintf(sendline, "no");
		write(sockfd, sendline, strlen(sendline));
	} else {
		sprintf(sendline, "ok");
		write(sockfd, sendline, strlen(sendline));
		int filesize = fileSizeMap[fileName];
		int ownerNum = fileUserList[fileName].size();
		int packetNum = filesize / 512;
		if (filesize % 512 > 0) packetNum++;
		int offset = packetNum / ownerNum;
		std::vector<std::string> &list = fileUserList[fileName];
		for (int i = 0; i < ownerNum; i++) {
			int udpfd;
			struct sockaddr_in udpaddr;
			socklen_t len;
			len = sizeof(udpaddr);
			bzero(&udpaddr, sizeof(udpaddr));
			udpaddr.sin_family = AF_INET;
			int sourceFD = findUserFD(list[i]);
			udpaddr.sin_port = fdToCliaddr[sourceFD].sin_port;
			udpaddr.sin_addr = fdToCliaddr[sourceFD].sin_addr;
			udpfd = socket(AF_INET, SOCK_DGRAM, 0);
			char sendline[MAX] = {0};
			struct sockaddr_in &sin = fdToCliaddr[sockfd];
			int last = (i == ownerNum - 1) ? 1 : 0;
			if (i == ownerNum - 1) sprintf(sendline, "upload %s %d %d %s %d %d\n", filename, i * offset, packetNum, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), last);
			else sprintf(sendline, "upload %s %d %d %s %d %d\n", filename, i * offset, (i + 1) * offset, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), last);
			sendto(udpfd, sendline, strlen(sendline), 0, (struct sockaddr *) &udpaddr, len);
			close(udpfd);
		}
		int udpfd;
		struct sockaddr_in udpaddr;
		socklen_t len;
		len = sizeof(udpaddr);
		bzero(&udpaddr, sizeof(udpaddr));
		udpaddr.sin_family = AF_INET;
		udpaddr.sin_port = fdToCliaddr[sockfd].sin_port;
		udpaddr.sin_addr = fdToCliaddr[sockfd].sin_addr;
		udpfd = socket(AF_INET, SOCK_DGRAM, 0);
		char sendline[MAX] = {0};
		sprintf(sendline, "download %s %d\n", filename, filesize);
		sendto(udpfd, sendline, strlen(sendline), 0, (struct sockaddr *) &udpaddr, len);
		close(udpfd);
	}
	pthread_mutex_unlock(&mutex);
}

void sendUserIPPort(int sockfd, char *username) {
	pthread_mutex_lock(&mutex);
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
	pthread_mutex_unlock(&mutex);
}

void sendFileList(int sockfd) {
	pthread_mutex_lock(&mutex);
	char sendline[MAX] = {0};
	sprintf(sendline, "Files on server are:");
	for (auto file : fileSet) {
		strcat(sendline, "\n");
		strcat(sendline, file.data());
	}
	strcat(sendline, "\n");
	write(sockfd, sendline, strlen(sendline));
	pthread_mutex_unlock(&mutex);
}

void sendUserList(int sockfd) {
	pthread_mutex_lock(&mutex);
	char sendline[MAX] = {0};
	sprintf(sendline, "Online users are:");
	for (auto user : onlineUserList) {
		strcat(sendline, "\n");
		strcat(sendline, user.data());
	}
	strcat(sendline, "\n");
	write(sockfd, sendline, strlen(sendline));
	pthread_mutex_unlock(&mutex);
}

void mergeFileList() {
	pthread_mutex_lock(&mutex);
	fileSet.clear();
	fileUserList.clear();
	for (auto user : onlineUserList) {
		for (auto file : userFileList[user]) {
			fileSet.insert(file);
			fileUserList[file].push_back(user);
		}
	}
	pthread_mutex_unlock(&mutex);
}

void readFileList(int sockfd, char *input) {
	pthread_mutex_lock(&mutex);
	std::string userName = fdToUsername[sockfd];
	userFileList[userName].clear();
	char *token = strtok(input, " ");
	token = strtok(NULL, " ");
	while (token) {
		std::string dataName = token;
		userFileList[userName].push_back(dataName);
		token = strtok(NULL, " ");
		std::string dataSize = token;
		fileSizeMap[dataName] = atoi(dataSize.data());
		token = strtok(NULL, " ");
	}
	puts("---------------");
	printf("User %s has:\n", userName.data());
	for (unsigned i = 0; i < userFileList[userName].size(); i++) {
		printf("%s, size: %d\n", userFileList[userName][i].data(), fileSizeMap[userFileList[userName][i]]);
	}
	puts("---------------");
	pthread_mutex_unlock(&mutex);
}

inline void removeOnlineStatus(int sockfd) {
	pthread_mutex_lock(&mutex);
	if (fdToUsername[sockfd] == "") printf("User unknown logged out.\n");
	else printf("User %s logged out.\n", fdToUsername[sockfd].data());
	unsigned idxToDelete;
	for (idxToDelete = 0; idxToDelete < onlineUserList.size(); idxToDelete++)
		if (onlineUserList[idxToDelete] == fdToUsername[sockfd]) {
			onlineUserList.erase(onlineUserList.begin() + idxToDelete);
			break;
		}
	fdToUsername[sockfd] = "";
	pthread_mutex_unlock(&mutex);
}

void deleteAccount(int sockfd) {
	pthread_mutex_lock(&mutex);
	char sendline[MAX] = {0};
	std::string userName = fdToUsername[sockfd];
	userAndPassword[userName] = "";
	puts("A user just deleted his/her account.");
	sprintf(sendline, "User %s is deleted, logged out.\n", userName.data());
	write(sockfd, sendline, strlen(sendline));
	pthread_mutex_unlock(&mutex);
}

void registerAccount(int sockfd, char *username, char *password) {
	pthread_mutex_lock(&mutex);
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
	pthread_mutex_unlock(&mutex);
}

void loginAccount(int sockfd, char *username, char *password) {
	pthread_mutex_lock(&mutex);
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
	pthread_mutex_unlock(&mutex);
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
		} else if (!strcmp("DF", command)) {
			char filename[100] = {0};
			sscanf(recv, "%*s%s", filename);
			initialDownload(connfd, filename);
		} else if (!strcmp("stop", command)) {
			char filename[100] = {0};
			sscanf(recv, "%*s%s", filename);
			stopDownload(connfd, filename);
		} else if (!strcmp("UF", command)) {
			
		}
		bzero(recv, sizeof(recv));
	}
	removeOnlineStatus(connfd);
	mergeFileList();
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
		pthread_mutex_lock(&mutex);
		fdToCliaddr[connfd] = cliaddr;
		pthread_mutex_unlock(&mutex);
		printf("Connection from: %s, port: %d.\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		pthread_create(&tid, NULL, &run, (void *) &connfd);
	}

	return 0;
}
