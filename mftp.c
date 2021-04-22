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
#include <unistd.h>

int putFile(char* path, char* hostname, int socketfd);
int getFile(char* path, char* hostname, int socketfd);
int quit(int);
int serverList(char*, int);
int list();
int changeServerDirectory(char*, int);
int changeDirectory(char*);
int getSocket(char*, char*);
int getDataConnection(char*, int);
int showFileContents(char* path, char* hostname, int socketfd);
int errorPrint(char*);
int readFromServer(int, char*);
int writeToServer(int, char*);


int main(char argc, char ** argv) {
	if(argc < 3) {
		printf("Usage: %s <port #> <host name>\n",argv[0]);
		return -1;	
	}
	
	int err;
	char * hostname = argv[2];
	char * portnum = argv[1];
	int socketfd;
	struct addrinfo hints, *actualdata;
	char* command = NULL;
	char* argument = NULL;
	const char delimiter[2] = " ";
	char buffer[512];

	memset(&hints, 0, sizeof(hints));
	socketfd = getSocket(hostname, portnum);
	
	printf("mftp>");
	while(fgets(buffer, 512, stdin) != NULL) {	
		buffer[strlen(buffer) - 1] = '\0';
		command = strtok(buffer, delimiter);
		argument = strtok(NULL, delimiter);
		if(command == NULL) {
			printf("mftp>");
			continue;
		}

		if(!strcmp(command, "exit")) {
			// Quit command
			quit(socketfd);
			return 0;
		}
		else if(!strcmp(command, "ls")) {
			list();
			// ls command
		}
		else if(!strcmp(command, "rls")) {
			// rls command
			serverList(hostname, socketfd);
		}
		else if(!strcmp(command, "cd")) {
			if(argument == NULL) {
				printf("Command error: expecting a parameter\n");
				printf("mftp>");
				continue;
			}
			changeDirectory(argument);
			// cd command
		}
		else if(!strcmp(command, "rcd")) {
                        if(argument == NULL) {
                                printf("Command error: expecting a parameter\n");
				printf("mftp>");
                                continue;
                        }
                        // rcd command
			changeServerDirectory(argument, socketfd);
                }
		else if(!strcmp(command, "get")) {
                        if(argument == NULL) {
                                printf("Command error: expecting a parameter\n");
				printf("mftp>");
                                continue;
                        }
                        // get command
			getFile(argument, hostname, socketfd);
                }
		else if(!strcmp(command, "put")) {
			if(argument == NULL) {
				printf("Command error: expecting a parameter\n");
				printf("mftp>");
				continue;
			}
			// put command
			putFile(argument, hostname, socketfd);
		}
		else if(!strcmp(command, "show")) {
                        if(argument == NULL) {
                                printf("Command error: expecting a parameter\n");
				printf("mftp>");
                                continue;
                        }
                        // show command
			showFileContents(argument, hostname, socketfd);
                }
		else {
			printf("Command '%s' is unknown - ignored\n",command);
			printf("mftp>");
		}
		printf("mftp>");
	}

			
	//getFile("testfile.txt", hostname, socketfd);
	//showFileContents("mftp.c", "localhost", socketfd);
	//changeServerDirectory("..", socketfd);
	//serverList(hostname, socketfd);
	quit(socketfd);	
	//changeDirectory("..");
	//list();
	return 0;
	
}

int writeToServer(int fd, char* message) {
	err = write(fd, message, strlen(message));
	if(err < 0) {
		perror("write to server");
		return -1;
	}
	return 0;
}

int readFromServer(int fd, char* storage, int storageSize) {
	char buffer[32] = "";
	int err;
	int index = 0;
	while(buffer[0] != '\n' && index < storageSize) {
		err = read(fd, buffer, 1);
		if(err < 0) {
			perror("read from server");
			return -1;
		}
		storage[index] = buffer[0];
		index++;
	}
	return 0;
}

int errorPrint(char* error) {
	if(error[0] != 'E') {
		return -1;
	}
	char message[512];
	int index = 0;
	for(int i = 1; i < strlen(error); i++) {
		message[i-1] = error[i];
		index++;
	}
	message[index] = '\0';
	fprintf("Error recieved from server: %s", message);
	return 0;
}

int getSocket(char* hostname, char* portnum) {
	int socketfd, err;
	struct addrinfo hints, *actualdata;

	printf("Attempting connecton on port: %s\n", portnum);
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_INET;

        err = getaddrinfo(hostname, portnum, &hints, &actualdata);
        if(err != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
                return -2;
        }

        socketfd = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
	if(socketfd == -1) {
                perror("Socket");
                return -errno;
        }

        err = connect(socketfd, actualdata->ai_addr, actualdata->ai_addrlen);
        if(err < 0) {
                perror("Connect");
                return -errno;
        }

	return socketfd;
}

int getDataConnection(char* hostname, int mainConnection) {
	char portnum[8];
	int err;
	writeToServer(mainConnection, "D\n");
	char response[512];
	int index = 0;
	readFromServer(mainConnection,response,512);
	if(response[0] == 'A') {
		int index = 1;
		while(response[index] != '\n' && index < strlen(response)) {
			portnum[index - 1] = response[index];
			index++;
		}
		portnum[index - 1] = '\0';
	}
	else if(response[0] == 'E') {
		errorPrint(response);
		return -1;
	}
	else {
		fprintf(stderr, "Error: Sever sent unexpected response %s\n", buffer);
		return -1;
	}

	int newSocketfd = getSocket(hostname, portnum);
	printf("new port: %s\n",portnum);
	return newSocketfd;	
}

int putFile(char* path, char* hostname, int mainConnection){
	// Open file
	int err;
	char name[256] = "";
	char buffer[512];
	int fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		perror("open failed");
		return -1;
	}
	// Open data connection
	int dataConnection = getDataConnection(hostname, mainConnection);
	if(dataConnection == -1){
		close(fd);
		return -1;
	}
	// Get file name
	int index = 0;
	for(int i = 0; (i < strlen(path)) && (index < 256); i++) {
		if(path[i] == '/') {
			index = 0;
		}
		else {
			name[index] = path[i];
			index ++;
		}
	}
	name[index] = '\0';
	// Send P<pathname>
	err = write(mainConnection, "P", 1);
	if(err < 0) {
		perror("write");
		close(fd);
		return -1;
	}
	err = write(mainConnection, name, strlen(name));
	if(err < 0) {
		perror("write");
		close(fd);
		return -1;
	}
	err = write(mainConnection, "\n", 1);
	if(err < 0) {
		perror("write");
		close(fd);
		return -1;
	}
	// Check response
	err = read(mainConnection, buffer, 100);
	if(err < 0) {
		perror("read");
		close(fd);
		return -1;
	}
	if(buffer[0] == 'A') {
		printf("Server accepted command\n");
	}
	else if(buffer[0] == 'E') {
		errorPrint(buffer);
		close(fd);
		return -1;
	}
	else {
		fprintf(stderr, "Server Error: Server sent unexpected response");
		close(fd);
		return -1;
	}
	// Send data
	int numRead;
	while( (numRead = read(fd, buffer, 511)) > 0) {
		printf("numread= %d\n",numRead);
		err = write(dataConnection, buffer, numRead);
		if(err < 0) {
			perror("write");
			close(fd);
			return -1;
		}
	}
	if(numRead < 0 ){
		perror("Read");
		close(dataConnection);
		close(fd);
		return -1;
	}
	close(dataConnection);
	close(fd);
	return 0;
}

int getFile(char* path, char* hostname, int mainConnection){
	//get file name
	int err;
	int index = 0;
	char name[256] = "";
	char buffer[100];
	for(int i = 0; (i < strlen(path)) && (index < 256); i++){
		if(path[i] == '/') {
			index = 0;
		}
		else {
			name[index] = path[i];
			index ++;
		}
	}
	name[index] = '\0';
	//check if file exists already
	int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, S_IRWXU);
	if(fd == -1) {
		if(errno == EEXIST) {
			fprintf(stderr, "File exists\n");
			return 0;
		}
		else {
			perror("open failed");
			return -1;
		}
	}	
	int dataConnection = getDataConnection(hostname, mainConnection);
	//open data connection
	//write contents to file
	//cleanup
	
	err = write(mainConnection, "G", 1);
        if(err < 0) {
                perror("write");
		close(fd);
                return -1;
        }

        err = write(mainConnection, path, strlen(path));
        if(err < 0) {
                perror("write");
		close(fd);
                return -1;
        }

        err = write(mainConnection, "\n", 1);
        if(err < 0) {
                perror("write");
		close(fd);
                return -1;
        }
	err = read(mainConnection, buffer, 100);
        if(err < 0) {
                perror("read");
		close(fd);
                return -1;
        }

        if(buffer[0] == 'A') {
                printf("Server accepted command\n");
        }
        else if(buffer[0] == 'E') {
		errorPrint(buffer);
		close(fd);
                return -1;
        }
	else {
		fprintf(stderr, "Servor Error: Server sent unexpected response\n");
		close(fd);
		return -1;
	}
		
	int numRead;
	while( (numRead = read(dataConnection, buffer, 100)) > 0) {
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

int quit(int mainConnection) {
	int err;
	char buffer[100];

	err = write(mainConnection, "Q\n", 2);
	if(err < 0) {
		perror("write");
		return -1;
	}
	
	err = read(mainConnection, buffer, 10);
	if(err < 0) {
		perror("read");
		return -1;
	}

	if(buffer[0] == 'A') {
		printf("Successfully quit from server\n");
		return 0;
	}
	else if(buffer[0] == 'E') {
		errorPrint(buffer);
		return -1;
	}
	else {
		fprintf(stderr, "Server error: Server sent unexpected response\n");
		return -1;
	}

	return 0;
}

int showFileContents(char* path, char* hostname, int mainConnection) {
	// Establish data connection
	int dataConnection = getDataConnection(hostname, mainConnection);
	// Send G command to get file
	int err;

	err = write(mainConnection, "G", 1);
	if(err < 0) {
		perror("write");
		return -1;
	}

	err = write(mainConnection, path, strlen(path));
	if(err < 0) {
		perror("write");
		return -1;
	}

	err = write(mainConnection, "\n", 1);
	if(err < 0) {
		perror("write");
		return -1;
	}

	char buffer[100];
	err = read(mainConnection, buffer, 100);
	if(err < 0) {
		perror("read");
		return -1;
	}

	if(buffer[0] == 'A') {
		printf("Server accepted command\n");	
	}
	else if(buffer[0] == 'E') {
		errorPrint(buffer);
		return -1;
	}
	else {
		fprintf(stderr, "Server Error: Server sent unexpected response\n");
		return -1;
	}

	// Pipe returned contents from socket into more
	pid_t child = fork();
	if(child == -1) {
		perror("fork");
		return -1;
	}

	if(!child) { // Child process
		dup2(dataConnection, 0);
		execlp("more", "more", "-20", (char *) NULL);
		perror("execlp");
		return -1;
	}
	// Wait for child to finish
	err = wait(NULL);
	if(err == -1) {
		perror("wait");
		return -1;
	}

	return 0;
}


int changeServerDirectory(char* newPath, int mainConnection) {
	int err;
	char buffer[100];

	err = write(mainConnection, "C", 1);
	if(err < 0) {
		perror("Write");
		return -1;
	}

	err = write(mainConnection, newPath, strlen(newPath));
        if(err < 0) {
                perror("Write");
                return -1;
        }

	err = write(mainConnection, "\n", 1);
	if(err < 0) {
                perror("Write");
                return -1;
        }

	err = read(mainConnection, buffer, 100);
	if(err < 0) {
		perror("Read");
		return -1;
	}

	if(buffer[0] == 'A') {
		printf("Successfully changed server directory.\n");
		return 0;
	}
	else if(buffer[0] == 'E') {
		errorPrint(buffer);
		return -1;
	}
	else {
		fprintf(stderr, "Error: Server returned unexpected response.\n");
		return -1;
	}

	return 0;
}


int changeDirectory(char* newPath) {
	int err;

	err = chdir(newPath);
	if(err == -1) {
		perror("chdir");
		return -1;
	}
	return 0;
}

int serverList(char* hostname, int mainConnection) {
	int dataConnection = getDataConnection(hostname, mainConnection);
	int err;
	char buffer[100];

	err = write(mainConnection, "L\n", 2);
	if (err < 0) {
		perror("Write");
		return -1;
	}

	err = read(mainConnection, buffer, 100);
	if (err < 0) {
		perror("Read");
		return -1;
	}

	if(buffer[0] == 'E') {
		errorPrint(buffer);
		return -1;
	}
	else if(buffer[0] == 'A') {
		// success
		pid_t child = fork();
		if(child == -1) {
			perror("Fork");
			return -1;
		}

		if(!child) { // Child process
			dup2(dataConnection,0);
                        execlp("more","more","-5", (char *) NULL);
                        perror("execlp");
                        return -1;
		}
		else { // Parent
			err = wait(NULL);
			if(err == -1) {
				perror("Wait");
				return -1;
			}
			return 0;
		}
	}
	else {
		fprintf(stderr, "Error: Server sent invalid response\n");
		return -1;
	}

	return 0;
}

int list() {
	int wstatus;
	int err;

	pid_t id = fork();
	if(id == -1) {
		perror("Fork");
		return errno;
	}

	if(!id) {// Child
		int fd[2];

		err = pipe(fd);
		if(err == -1) {
			perror("pipe");
			return errno;
		}

		pid_t youngerChild = fork();
		if(youngerChild == -1) {
			perror("Fork");
			return errno;
		}

		if(youngerChild) {// Older Child
			// Here we get the ls output from the younger child and exec more
			err = wait(NULL);
			if(err == -1) {
                		perror("wait");
        			return -1;
			}

			err = close(fd[1]);
			if(err == -1) {
				perror("close");
				return errno;
			}

			err = dup2(fd[0],0);
			if(err == -1) {
				perror("dup2");
				return errno;
			}

			execlp("more", "more", "-5", (char *) NULL);
			perror("execvp child");
			return -1;

		}
		else {// Younger Child
			// Here we execute ls and pipe it to the older child
			err = close(fd[0]);
			if(err == -1) {
                                perror("close");
                                return errno;
                        }

                        err = dup2(fd[1],1);
			if(err == -1) {
                                perror("dup2");
                                return errno;
                        }

			execlp("ls", "ls", "-a", "-l", (char *) NULL);
                        perror("execvp child");
                        return -1;

		}
	}
	err = wait(NULL);
	if(err == -1) {
		perror("wait");
		return -1;
	}
	
	return 0;
}
