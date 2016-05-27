#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
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

int tcpfd;

struct upload_info {
	char filename[100], targetIP[100];
	int left, right, last, port, udpfd, status;
	int *ACKarr;
};

void showMenu() {
	puts("--------------------");
	puts("[T]alk");
	puts("[L]ogout");
	puts("[D]elete account");
	puts("[SU]Show User");
	puts("[SF]Show File");
	puts("[DF]Download File");
	puts("[UF]Upload File");
	puts("--------------------");
}

void sendFileList(int sockfd) {
	char sendline[MAX] = {0};
	sprintf(sendline, "FileList ");
	DIR *dp;
	struct dirent *ep;
	dp = opendir("./file/");
	if (dp != NULL) {
		while ((ep = readdir(dp))) {
			if (!strcmp(".", ep->d_name) || !strcmp("..", ep->d_name)) continue;
			strcat(sendline, " ");
			strcat(sendline, ep->d_name);
			FILE *pfile;
			int fileSize;
			char path[50] = {0};
			sprintf(path, "./file/");
			strcat(path, ep->d_name);
			pfile = fopen(path, "rb");
			fseek(pfile, 0, SEEK_END);
			fileSize = ftell(pfile);
			fclose(pfile);
			char stringfilesize[50] = {0};
			sprintf(stringfilesize, " %d", fileSize);
			strcat(sendline, stringfilesize);
		}
	}
	write(sockfd, sendline, strlen(sendline));
	puts("File list sent.");
}

inline int chkAllReceived(int *receivedpacket, int packetnum, int *nowpacket) {
	int allone = 1;
	(*nowpacket) = 0;
	for (int i = 0; i < packetnum; i++) {
		if (receivedpacket[i]) (*nowpacket)++;
		allone &= receivedpacket[i];
	}
	return allone;
}

void *checkACK(void *arg) {
	// Get the info from the parent thread.
	struct upload_info *info = (struct upload_info *) arg;
	int counter = 0, numOfAck = info->right - info->left;
	// Detach this thread.
	pthread_detach(pthread_self());
	// Start to receive the ACKs.
	while (1) {
		if (counter == numOfAck) {
			info->status = 1;
			puts("All of the ACKs have been received.");
			showMenu();
			return NULL;
		}
		char recv[MAX] = {0};
		struct sockaddr_in udpaddr;
		bzero(&udpaddr, sizeof(udpaddr));
		socklen_t len = sizeof(udpaddr);
		recvfrom(info->udpfd, recv, MAX, 0, (struct sockaddr *) &udpaddr, &len);
		puts(recv);
		int idx;
		sscanf(recv, "%*s%d", &idx);
		if (info->ACKarr[idx - info->left] != 1) {
			info->ACKarr[idx - info->left] = 1;
			counter++;
		}
	}
}

void *upload(void *data_in) {
	// Get the data from the parent thread.
	struct upload_info info = *((struct upload_info *) data_in);
	// Struct the target addr.
	struct sockaddr_in targetAddr;
	bzero(&targetAddr, sizeof(targetAddr));
	targetAddr.sin_family = AF_INET;
	targetAddr.sin_port = htons(info.port);
	inet_pton(AF_INET, info.targetIP, &targetAddr.sin_addr);
	socklen_t len = sizeof(targetAddr);
	// Get a new udp port.
	info.udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	// New the ACK array.
	int numOfAck = info.right - info.left;
	info.ACKarr = new int[numOfAck];
	bzero(info.ACKarr, sizeof(int) * numOfAck);
	// Create a new thread to receive the ACKs.
	pthread_t tid;
	pthread_create(&tid, NULL, &checkACK, &info);
	// Start to send file.
	while (1) {
		if (info.status == 0) {
			char path[100] = {0};
			sprintf(path, "./file/");
			strcat(path, info.filename);
			FILE *fp = fopen(path, "rb");
			rewind(fp);
			for (int i = 0; i < info.left * 512; i++)
				fgetc(fp);
			for (int i = info.left; i < info.right; i++) {
				if (info.ACKarr[i - info.left] == 1) {
					if (i != info.right - 1 || info.last != 1) {
						for (int j = 0; j < 512; j++) fgetc(fp);
						continue;
					}
				}
				if (i == info.right - 1 && info.last == 1) {
					char sendline[MAX] = {0}, c;
					sprintf(sendline, "%10d ", i);
					int j = 0;
					while ((c = fgetc(fp)) != EOF) {
						sendline[11 + j] = c;
						j++;
					}
					sendto(info.udpfd, sendline, MAX, 0, (struct sockaddr *) &targetAddr, len);
				} else {
					char sendline[MAX] = {0};
					sprintf(sendline, "%10d ", i);
					for (int j = 0; j < 512; j++)
						sendline[11 + j] = fgetc(fp);
					sendto(info.udpfd, sendline, MAX, 0, (struct sockaddr *) &targetAddr, len);
				}
			}
			fclose(fp);
		} else if (info.status == 1) break;
	}
	delete[] info.ACKarr;
	return NULL;
}

void *run(void *udpfd_in) {
	int udpfd = *((int *) udpfd_in);
	char recv[MAX] = {0}, command[MAX] = {0};
	struct upload_info info;
	struct sockaddr_in incomeudpaddr;
	bzero(&incomeudpaddr, sizeof(incomeudpaddr));
	socklen_t len;
	pthread_detach(pthread_self());
	while (1) {
		len = sizeof(incomeudpaddr);
		recvfrom(udpfd, recv, MAX, 0, (struct sockaddr *) &incomeudpaddr, &len);
		sscanf(recv, "%s", command);
		if (!strcmp("download", command)) {
			printf("%s", recv);
			char filename[MAX] = {0};
			int filesize, packetnum;
			sscanf(recv, "%*s%s%d", filename, &filesize);
			packetnum = filesize / 512;
			if (packetnum % 512 > 0) packetnum++;
			FILE *fp;
			char path[100] = {0};
			sprintf(path, "./file/");
			strcat(path, filename);
			fp = fopen(path, "wb");
			int nowpacket = 0;
			int *receivedpacket = new int[packetnum];
			char *filedata = new char[packetnum * 512];
			bzero(filedata, sizeof(char) * packetnum * 512);
			bzero(receivedpacket, sizeof(int) * packetnum);
			while (!chkAllReceived(receivedpacket, packetnum, &nowpacket)) {
				recvfrom(udpfd, recv, MAX, 0, (struct sockaddr *) &incomeudpaddr, &len);
				int idx;
				sscanf(recv, "%d", &idx);
				char *datapointer = recv + 11;
				for (int i = 0; i < 512; i++) {
					filedata[idx * 512 + i] = *datapointer;
					datapointer++;
				}
				receivedpacket[idx] = 1;
				char sendline[MAX] = {0};
				sprintf(sendline, "ACK %d\n", idx);
				sendto(udpfd, sendline, MAX, 0, (struct sockaddr *) &incomeudpaddr, len);
				bzero(recv, sizeof(recv));
				printf("\rDownload %d %%.\n", nowpacket * 100 / packetnum);
				puts("[P]ause [C]ontinue [E]xit");
				fflush(stdout);
			}
			fwrite(filedata, sizeof(char), filesize, fp);
			fclose(fp);
			/*if (mypause == 2) remove(path);*/
			sendFileList(tcpfd);
			delete[] receivedpacket;
			delete[] filedata;
			/*if (mypause == 0) */printf("%s downloaded successfully.\n", filename);
		} else if (!strcmp("upload", command)) {
			printf("%s", recv);
			pthread_t tid;
			bzero(&info, sizeof(info));
			sscanf(recv, "%*s%s%d%d%s%d%d", info.filename, &info.left, &info.right, info.targetIP, &info.port, &info.last);
			pthread_create(&tid, NULL, &upload, &info);
		} else if (!strcmp("chat", command)) {
			char *place = recv + 5;
			printf("    %s", place);
		}
		bzero(recv, sizeof(recv));
		bzero(command, sizeof(command));
	}
	return NULL;
}

void chat(char *myusername, char *targetuserIP, int targetuserport) {
	int sockfd;
	struct sockaddr_in servaddr;
	socklen_t len;
	len = sizeof(servaddr);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(targetuserport);
	inet_pton(AF_INET, targetuserIP, &servaddr.sin_addr);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	char sendline[MAX] = {0}, buffer[MAX] = {0};
	sprintf(sendline, "chat %s: ", myusername);
	while ((fgets(buffer, MAX, stdin) != NULL)) {
		strcat(sendline, buffer);
		sendto(sockfd, sendline, strlen(sendline), 0, (struct sockaddr *) &servaddr, len);
		bzero(buffer, sizeof(buffer));
		bzero(sendline, sizeof(sendline));
		sprintf(sendline, "chat %s: ", myusername);
	}
	close(sockfd);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		puts("Usage: a.out <ServerIP> <ServerPort>");
		exit(0);
	}

	mkdir("./file", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	char sendline[MAX], command[MAX], recv[MAX], username[100] = {0};
	int sockfd, port, myudpfd;
	struct sockaddr_in servaddr, sin, myudpaddr;
	socklen_t len = sizeof(sin);
	pthread_t tid;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	tcpfd = sockfd;
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
		port = ntohs(sin.sin_port);
	}

	/*****/
	myudpfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&myudpaddr, sizeof(myudpaddr));
	myudpaddr.sin_family = AF_INET;
	myudpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myudpaddr.sin_port = htons(port);

	bind(myudpfd, (struct sockaddr *) &myudpaddr, sizeof(myudpaddr));
	pthread_create(&tid, NULL, &run, (void *) &myudpfd);
	/****/

	puts("**********Welcome**********");
	puts("[R]egister [L]ogin");
	bzero(sendline, sizeof(sendline));
	fgets(sendline, MAX, stdin);
	if (!strcmp("R\n", sendline)) {
		puts("\nPlease enter a new pair of username and password.");
		bzero(command, sizeof(command));
		fgets(command, MAX, stdin);
		sscanf(command, "%s", username);
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
		puts("\nPlease enter an existing pair of username and password.");
		bzero(command, sizeof(command));
		fgets(command, MAX, stdin);
		sscanf(command, "%s", username);
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
	sendFileList(sockfd);
	printf("\n**********Hello %s**********\n", username);
	while (1) {
		puts("You can input the following commands:");
		showMenu();
		puts("Your input:");
		bzero(sendline, sizeof(sendline));
		bzero(command, sizeof(command));
		bzero(recv, sizeof(recv));
		if (fgets(sendline, MAX, stdin) == NULL) break;
		if (!strcmp("L\n", sendline)) {
			printf("User %s logged out.\n", username);
			break;
		} else if (!strcmp("D\n", sendline)) {
			write(sockfd, sendline, strlen(sendline));
			read(sockfd, recv, MAX);
			printf("%s", recv);
			break;
		} else if (!strcmp("SU\n", sendline)) {
			write(sockfd, sendline, strlen(sendline));
			read(sockfd, recv, MAX);
			printf("%s", recv);
		} else if (!strcmp("SF\n", sendline)) {
			write(sockfd, sendline, strlen(sendline));
			read(sockfd, recv, MAX);
			printf("%s", recv);
		} else if (!strcmp("T\n", sendline)) {
			puts("Who do you want to talk to?");
			fgets(command, MAX, stdin);
			strcat(sendline, command);
			write(sockfd, sendline, strlen(sendline));
			read(sockfd, recv, MAX);
			char targetuserIP[100] = {0};
			int targetuserport;
			sscanf(recv, "%*s%s%d", targetuserIP, &targetuserport);
			chat(username, targetuserIP, targetuserport);
		} else if (!strcmp("DF\n", sendline)) {
			puts("What do you want to download?");
			fgets(command, MAX, stdin);
			strcat(sendline, command);
			write(sockfd, sendline, strlen(sendline));
			read(sockfd, recv, MAX);
			char rep[100] = {0};
			sscanf(recv, "%s", rep);
			if (!strcmp("ok", rep)) puts("File is sending.");
			else puts("File not found.");
		} else if (!strcmp("UF\n", sendline)) {
			puts("What do you want to upload?");
			fgets(command, MAX, stdin);
			strcat(sendline, command);
			bzero(command, sizeof(command));
			puts("Who do you want to send to?");
			fgets(command, MAX, stdin);
			strcat(sendline, command);
			write(sockfd, sendline, strlen(sendline));
			read(sockfd, recv, MAX);
			char rep[100] = {0};
			sscanf(recv, "%s", rep);
			if (!strcmp("ok", rep)) {
				puts("File is sending.");
			} else puts("File not found.");
		}
	}

	return 0;
}
