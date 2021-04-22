#include "mftp.h"

/**************************************************
 *Writeen by: Brandon Huang
 *For: cs360 final project
 *Last modififed: 4/22/21
 *
 * Description: A program that acts as a server for the purposes of interacting with
 * the mftp client.
 * Usage: mftpserve
 * Interface Commands:
 * 	D: Establishes a dataconnection with the client
 * 	Q: Acknowledges the clients quit request and closes connections
 * 	C<path>: Changes the servers directory to the sent path
 * 	L: Prints the working directory of the server and sends it over the data connection
 * 		MUST HAVE PRIOR DATA CONNECTION SET UP
 * 	G<path>: Transfers a file from the given path to the client over the data connection
 * 		MUST HAVE PRIOR DATA CONNECTION SET UP
 * 	P<path>: Gets file from data connection from the client and names it the given path.
 * 		MUST HAVE PRIOR DATA CONNECTION SET UP
 *
**************************************************/

int main(char argc, char** argv){	
	int err, listenfd, wstatus;
	// Setup the first socket
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

	// Bind and listen on the socket
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

	// The main server is constantly looking for new connection and forking
	while(1) {
		printf("Looking for connection to accept\n");
		// Attempt to accept a new connection
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
		// Fork when new connection is established
		int pid = fork();

		if(pid == 0) { // Were a server child which handles connections
			fprintf(stdout, "New server child created\n");
			// The child begins communicating as a server with the client
			err = server(connectfd);
			if(err == -1) {
				fprintf(stderr, "Server closed unexpectedly\n");
				return -1;
			}
			fprintf(stdout, "Server child ending nomrally\n");
			return 0;
		}
		// Wait on children without hangs.
		waitpid(0, &wstatus, WNOHANG);

	}
	return 0;
}

// This function is where the forked children go once they establish a connection. This is where most of the communication happens.
int server(int connectfd) {
	char buffer[512];
	char command[512];
	char pathname[512];
	int hasDataConnection = 0;
	int dataConnection;
	int index;
	int err;
	// Always keep listening for requests
	while(1) {
		index = 0;
		strcpy(command,"");
		// Read for requests from the connectoin
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
		
		// Check for types of commands
		if(command[0] == 'Q') {
			// Q command
			serverQuit(connectfd);
			fprintf(stdout, "Client quit successfully.\n");
			return 0;
		}
		else if(command[0] == 'C') {
			// C command
			getPathname(command, pathname);
			serverChangeDirectory(pathname, connectfd);
		}
		else if(command[0] == 'D') {
			// D command
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
			// G command, reject it if a data connection hasn't been set up prior
			if(!hasDataConnection) {
				writeToClient(connectfd, "ENo prior data connection established\n");
				continue;
			}
			getPathname(command, pathname);
			err = serverGet(dataConnection, connectfd, pathname);
			close(dataConnection);
			hasDataConnection = 0;
		}
		else if(command[0] == 'P') {
			// P command, reject it if a data connection hasn't been set up prior
			if(!hasDataConnection) {
				writeToClient(connectfd, "ENo prior data connection established\n");
				continue;
			}
			getPathname(command, pathname);
			err = serverPut(dataConnection, connectfd, pathname);
			close(dataConnection);
			hasDataConnection = 0;
		}
		else if(command[0] == 'L') {
			// L command, reject if data connection hasn't been set up prior
			if(hasDataConnection) {
				err = writeToClient(connectfd, "A\n");
				if(err < 0) continue;
				serverls(dataConnection);
				close(dataConnection);
				hasDataConnection = 0;
			}
			else {
			       writeToClient(connectfd, "ENo prior data connection established\n");
			}
		}
		else {
			fprintf(stdout, "Unknown command recieved: %s",buffer);
			writeToClient(connectfd, "EUnknown command sent\n");
		}
	}
	return 0;
}

// This function simply write a given message to the given sockst. Returns 0 on success and -1 on failure
int writeToClient(int fd, char* message) {
	int err;
	//printf("**Writing to client: %s", message);
	err = write(fd, message, strlen(message));
	if(err < 0) {
		perror("write to client");
		return -1;
	}
	return 0;
}

// This function reads from the given socket until encountering a \n and stores it in the given storage.
// Also takes the size of the storage to prevent buffer overflow. Returns 0 on scucess and -1 on failure
int readFromClient(int fd, char* storage, int storageSize) {
	char buffer[32] = "";
	int err;
	int index = 0;
	while(buffer[0] != '\n' && index < storageSize) {
		err = read(fd, buffer, 1);
		if(err < 0) {
			perror("read from client");
			return -1;
		}
		storage[index] = buffer[0];
		index++;
	}
	storage[index] = '\0';
	//printf("**Read from client: %s", storage);
	return 0;
}

// This fucntions represents the P request. It attempts to communicate with the client across the main connection
// and transfer a file from the client through the data connection. Returns 0 on success and -1 on failure
int serverPut(int dataConnection, int mainConnection, char* pathname) {
	int err, numRead;
	char message[512];
	char buffer[512];
	// Open the file, checking if it already exists
	int fd = open(pathname, O_WRONLY | O_CREAT | O_EXCL, S_IRWXU);
	// If it does send an error to the client
	if(fd == -1) {
		strcpy(message,"E");
		strcat(message,strerror(errno));
		strcat(message,"\n");
		perror("Open: ");
		writeToClient(mainConnection, message);
		return -1;
	}
	// Otherwise accept it.
	err = writeToClient(mainConnection, "A\n");
	if(err < 0) return -1;
	// transfer the data from the client
	printf("Sending file %s to client\n",pathname);
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

// This function represents the G request. It attempts to communicate with the client accross the main connection
// and transfers a file to the client through the data connection. Returns a 0 on success and -1 on failurre
int serverGet(int dataConnection, int mainConnection, char* pathname) {
	int err, numRead;
	char buffer[512];
	char message[512];
	struct stat info;
	// Get file info
	err = lstat(pathname, &info);
	if(err < 0) {
		perror("lstat");
		strcpy(message,"E");
		strcat(message,strerror(errno));
		strcat(message,"\n");
		writeToClient(mainConnection, message);
		return -1;
	}
	// Check if its a directory
	if(S_ISDIR(info.st_mode)) {
		writeToClient(mainConnection, "EPath specified is directory\n");
		return -1;
	}
	// Now open the file, checking if it already exists on the server end
	int fd = open(pathname, O_RDONLY, 0);
	if(fd < 0) {
		perror("open");
		strcpy(message,"E");
		strcat(message,strerror(errno));
		strcat(message,"\n");
		writeToClient(mainConnection, message);
		return -1;
	}
	err = writeToClient(mainConnection, "A\n");
	if(err < 0) {
		close(fd);
		return -2;
	}
	// Get the file from the client.
	printf("Receiving file %s from client\n", pathname);
	while( (numRead = read(fd, buffer, 511)) > 0) {
		err = write(dataConnection, buffer, numRead);
		if(err < 0) {
			perror("write");
			close(fd);
			return -2;
		}
		//printf("Writing %d bytes...\n", numRead);
	}
	close(fd);
	return 0;
}

// This function represents the L request. Attempts to send information of the current working directory of
// the server accross the given data connectoin. Returns 0 o success and -1 on failure
int serverls(int dataConnection) {
	int wstatus, err;
	// Fork and have the child exec into ls
	pid_t id = fork();
	if(id == -1) {
		perror("Fork");
		return -1;
	}

	if(!id) { //child
		// Replace stdout file desciptor with the socket
		err = dup2(dataConnection, 1);
		if(err == -1) {
			perror("dup2");
			return -1;
		}
		execlp("ls", "ls", "-a", "-l", (char *) NULL);
		perror("execlp failed");
		return -1;

	}

	// wait for the child and return
	err = wait(NULL);
	if(err == -1); {
		return -1;
	}
	return 0;
}

// This function represents the D request. Attempts to establish a data connection with the client and communicate
// about it across the main connection. returns 0 on success and -1 on failure.
int serverDataConnection(int mainConnection) {
	// Set up the new socket
	int newDataConnection = socket(AF_INET, SOCK_STREAM, 0);
	int port, err;
	char portString[16] = "";
	if(newDataConnection == -1) {
		perror("socket");
		return -1;
	}

	// Get info, and a port number using the port number as 0. So it will be assigned any open port.
	struct sockaddr_in newAddress;
	memset(&newAddress, 0, sizeof(newAddress));
	newAddress.sin_family = AF_INET;
	newAddress.sin_port = htons(0);
	newAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	// Attempt to bind
	err = bind(newDataConnection, (struct sockaddr*) &newAddress, sizeof(newAddress));
	if(err < 0) {
		perror("bind");
		return -1;
	}
	
	// Get the assigned info like portnumber
	struct sockaddr_in info;
	memset(&info, 0, sizeof(info));
	int len = sizeof(info);
	err = getsockname(newDataConnection,(struct sockaddr*) &info, &len);
	if(err == -1) {
		perror("getsockname");
	}
	// Convert the given port number into an int, then a string
	port = ntohs(info.sin_port);
	sprintf(portString, "%d", port);
	// Write to the client: A<portnum>\n
	err = write(mainConnection, "A", 1);
	err = write(mainConnection, portString, strlen(portString));
	err = write(mainConnection, "\n", 1);
	err = listen(newDataConnection, 1);
	if(err < 0) {
		perror("listen");
		return -1;
	}
	// Accept the new connection and return the new file desciptor
	struct sockaddr_in clientAddress;
	int connectedfd = accept(newDataConnection, (struct sockaddr*) &clientAddress, &len);
	if(connectedfd < 0) {
		perror("accept");
		return -1;
	}
	// printf("New data connection on fd: %d (ndc:%d) and port: %d\n", connectedfd, newDataConnection, port);
	return connectedfd;
}

// This functoin repressents the C request. Communicates through the main connection and attempts to change the servers working directory
// to the given path. Returns 0 on success and -1 on failure.
int serverChangeDirectory(char* pathname, int connectfd){
	int err;
	char message[512];
	// Change directory
	err = chdir(pathname);
	// IF theres an error, let the client know
	if(err == -1) {
		perror("chdir");
		strcpy(message, "E");
		strcat(message, strerror(errno));
		strcat(message, "\n");
		writeToClient(connectfd, message);
		return -1;
	}
	fprintf(stdout, "Directory successfully changed to %s\n",pathname);
	err = writeToClient(connectfd, "A\n");
	if(err < 0) return -1;
	return 0;
}

// This function quicly seperates the path name from other characcters and stores it in the given string.
// Returns 0 on success and -1 on failure
char* getPathname(char* command, char* pathname){
	int index = 0;
	// Remove the first character from the string, skipping \n
	for(int i = 1; i < strlen(command); i++) {
		if(command[i] == '\n') {
			continue;
		}
		pathname[i-1] = command[i];
		index++;
	}
	// add a null terminator
	pathname[index] = '\0';
	return pathname;
}

// This function represnets the Q request. Simply acknowledges the client accross the main connectoin.
// Returns 0 on success and -1 on failure
int serverQuit(int connectfd) {
	int err = write(connectfd, "A\n", 2);
	if (err < 0) {
		perror("write");
		return -1;
	}
	return 0;
}

