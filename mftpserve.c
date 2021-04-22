#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int serverGet(int dataConnection, int mainConnection, char* path);
int serverPut(int dataConnection, int mainConnection, char* path);
int serverls(int);
int serverDataConnection(int);
int serverChangeDirectory(char*, int);
char* getPathname(char* command, char* pathname);
int server(int);
int serverQuit(int);

int main(char argc, char** argv){	
	int err, listenfd, wstatus;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	err = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if(err < 0) { 
		perror("setsockopt failed");
		return -1;
	}

	struct sockaddr_in serverAddress;
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(49999);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	err = bind(listenfd, (struct sockaddr*) &serverAddress, sizeof(serverAddress));
	if(err < 0) { 
		perror("Bind failed ");
		return -1;
	}
	err = listen(listenfd, 4);
	if(err < 0) {
		perror("Listen failed");
		return -1;
	}

	int connectfd;
	struct sockaddr_in clientAddress;
	char nameBuffer[NI_MAXHOST];
	int length = sizeof(struct sockaddr_in);

	while(1) {
		printf("Looking for connection to accept\n");
		connectfd = accept(listenfd, (struct sockaddr*) &clientAddress, &length);
		if(connectfd < 0) {
			perror("Accept");
			continue;
		}
		err = getnameinfo((struct sockaddr*) &clientAddress, sizeof(clientAddress), nameBuffer,
					sizeof(nameBuffer), NULL, 0, NI_NUMERICSERV);
		if(err != 0) {
			fprintf(stderr, "getnameinfo: %s\n", gai_strerror(err));
			continue;
		}
		int pid = fork();

		if(pid == 0) { // Were a server child which handles connection.s
			// Do server stuff
			// Keep waiting for commands
			fprintf(stdout, "New server child createdi\n");
			err = server(connectfd);
			if(err == -1) {
				fprintf(stderr, "Server closed unexpectedly\n");
				return -1;
			}
			fprintf(stdout, "Server child ending nomrally\n");
			return 0;
		}
		// Maybe wait for childs after a threshhold?
		waitpid(0, &wstatus, WNOHANG);

	}
	return 0;
}

int server(int connectfd) {
	char buffer[512];
	char command[512];
	char pathname[512];
	int hasDataConnection = 0;
	int dataConnection;
	int index;
	int err;
	while(1) {
		index = 0;
		strcpy(command,"");
		while((err = read(connectfd, buffer, 1)) > 0) {
			command[index] = buffer[0];
			index++;
			if(buffer[0] == '\n') {
				break;
			}
		}
		if(err == 0) {
			printf("EOF detected\n");
			return -1;
		}
		else if(err < 0) {
			perror("read");
			return -1;
		}
		if(!strcmp(command,"")) {
			continue;	
		}
		command[index] = '\0';
		printf("command sent: %s",command);
		
		// Check for types of commands
		if(command[0] == 'Q') {
			serverQuit(connectfd);
			fprintf(stdout, "Client quit successfully.\n");
			return 0;
		}
		else if(command[0] == 'C') {
			getPathname(command, pathname);
			serverChangeDirectory(pathname, connectfd);
		}
		else if(command[0] == 'D') {
			dataConnection = serverDataConnection(connectfd);
			if(dataConnection < 0) {
				printf("Problem setting up data connection\n");
				hasDataConnection = 0;
			}
			else {
				hasDataConnection = 1;
			}
		}
		else if(command[0] == 'G') {
			if(!hasDataConnection) {
				write(connectfd, "ENo prior data connection established\n", 38);
				continue;
			}
			getPathname(command, pathname);
			err = serverGet(dataConnection, connectfd, pathname);
			close(dataConnection);
			hasDataConnection = 0;
		}
		else if(command[0] == 'P') {
			if(!hasDataConnection) {
				write(connectfd, "ENo prior data connection established\n", 38);
				continue;
			}
			getPathname(command, pathname);
			err = serverPut(dataConnection, connectfd, pathname);
			close(dataConnection);
			hasDataConnection = 0;
		}
		else if(command[0] == 'L') {
			// Check if data connection is available
			if(hasDataConnection) {
				err = write(connectfd, "A\n", 2);
				if(err < 0) {
					perror("write");
					continue;
				}
				serverls(dataConnection);
				close(dataConnection);
				hasDataConnection = 0;
			}
			else { 
				write(connectfd, "E", 1);
				write(connectfd, "No prior data connection established", 36);
				write(connectfd, "\n", 1);
			}
		}
		else {
			fprintf(stdout, "Unknown command recieved: %s",buffer);
			write(connectfd, "EUnknown command sent\n", 22);
		}
	}
	return 0;
}

int serverPut(int dataConnection, int mainConnection, char* pathname) {
	int err, numRead;
	char buffer[512];
	int fd = open(pathname, O_WRONLY | O_CREAT | O_EXCL, S_IRWXU);
	if(fd == -1) {
		write(mainConnection, "E", 1);
		write(mainConnection, strerror(errno), strlen(strerror(errno)));
		write(mainConnection, "\n", 1);
		return -1;
	}
	err = write(mainConnection, "A\n", 2);
	if(err < 0) {
		perror("write");
		return -1;
	}
	while( (numRead = read(dataConnection, buffer, 511)) > 0) {
		err = write(fd, buffer, numRead);
		if(err < 0) {
			perror("write");
			close(fd);
			return -1;
		}
	}
	if(numRead < 0) {
		perror("read");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

int serverGet(int dataConnection, int mainConnection, char* pathname) {
	int err, numRead;
	char buffer[512];
	int fd = open(pathname, O_RDONLY, 0);
	if(fd < 0) {
		perror("open");
		write(mainConnection, "E", 1);
		write(mainConnection, strerror(errno), strlen(strerror(errno)));
		write(mainConnection, "\n",1);
		return -1;
	}
	err = write(mainConnection, "A\n", 2);
	if(err < 0) {
		perror("write");
		close(fd);
		return -2;
	}
	while( (numRead = read(fd, buffer, 511)) > 0) {
		err = write(dataConnection, buffer, numRead);
		if(err < 0) {
			perror("write");
			close(fd);
			return -2;
		}
		printf("Writing %d bytes...\n", numRead);
	}
	close(fd);
	return 0;
}

int serverls(int dataConnection) {
	int wstatus, err;
	pid_t id = fork();
	if(id == -1) {
		perror("Fork");
		return -1;
	}

	if(!id) { //child
		err = dup2(dataConnection, 1);
		if(err == -1) {
			perror("dup2");
			return -1;
		}
		execlp("ls", "ls", "-a", "-l", (char *) NULL);
		perror("execlp failed");
		return -1;

	}

	err = wait(NULL);
	if(err == -1); {
		perror("wait");
		return -1;
	}
	return 0;
}

int serverDataConnection(int mainConnection) {
	int newDataConnection = socket(AF_INET, SOCK_STREAM, 0);
	int port, err;
	char portString[16] = "";
	if(newDataConnection == -1) {
		perror("socket");
		return -1;
	}

	struct sockaddr_in newAddress;
	memset(&newAddress, 0, sizeof(newAddress));
	newAddress.sin_family = AF_INET;
	newAddress.sin_port = htons(0);
	newAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	err = bind(newDataConnection, (struct sockaddr*) &newAddress, sizeof(newAddress));
	if(err < 0) {
		perror("bind");
		return -1;
	}
	
	struct sockaddr_in info;
	memset(&info, 0, sizeof(info));
	int len = sizeof(info);
	err = getsockname(newDataConnection,(struct sockaddr*) &info, &len);
	if(err == -1) {
		perror("getsockname");
	}
	port = ntohs(info.sin_port);
	sprintf(portString, "%d", port);
	printf("Sending <A%s\n>",portString);
	err = write(mainConnection, "A", 1);
	err = write(mainConnection, portString, strlen(portString));
	err = write(mainConnection, "\n", 1);
	err = listen(newDataConnection, 1);
	if(err < 0) {
		perror("listen");
		return -1;
	}
	struct sockaddr_in clientAddress;
	int connectedfd = accept(newDataConnection, (struct sockaddr*) &clientAddress, &len);
	if(connectedfd < 0) {
		perror("accept");
		return -1;
	}
	printf("New data connection on fd: %d (ndc:%d) and port: %d\n", connectedfd, newDataConnection, port);
	return connectedfd;
}

int serverChangeDirectory(char* pathname, int connectfd){
	int err;
	err = chdir(pathname);
	if(err == -1) {
		perror("chdir");
		err = write(connectfd, "E", 1);
		if(err < 0) {
			perror("write");
			return -1;
		}
		err = write(connectfd, strerror(errno), strlen(strerror(errno)));
		if(err < 0) {
                        perror("write");
                        return -1;
                }
		err = write(connectfd, "\n", 1);
		if(err < 0) {
                        perror("write");
                        return -1;
                }
		return -1;
	}
	fprintf(stdout, "Directory successfully changed to %s\n",pathname);
	err = write(connectfd, "A\n", 2);
	if(err < 0) {
		perror("write");
		return -1;
	}
	return 0;
}

char* getPathname(char* command, char* pathname){
	int index = 0;
	for(int i = 1; i < strlen(command); i++) {
		if(command[i] == '\n') {
			continue;
		}
		pathname[i-1] = command[i];
		index++;
	}
	pathname[index] = '\0';
	return pathname;
}

int serverQuit(int connectfd) {
	int err = write(connectfd, "A\n", 2);
	if (err < 0) {
		perror("write");
		return -1;
	}
	return 0;
}

