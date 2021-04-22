#include "mftp.h"

/******************************************
 *Written by: Brandon Huang
 *For: cs360 final project
 *Last modified: 4/22/21
 *
 * Descirption: A program that provides a client user interface for the purposes
 * of interacting with the mftpserve server. 
 * Usage: mftp <port number> <address>
 * Commands:
 * 	quit: quits out of the client
 * 	cd <path>: changes clients working directory
 * 	rcd <path>: changes the servers working directory
 * 	ls: prints the clients working directory
 * 	rls: prints the servers working directory
 * 	show <path>: Prints the contents of a file of the given path from the
 * 	server's directory
 * 	get <path>: transfers a file from the given path from the server's
 * 	directory into the working directory of the client.
 * 	put <path>: transfers a file from the given path from the client's
 * 	directory into the working directory of the server.
******************************************/

int main(char argc, char ** argv) {
	if(argc < 3) {
		printf("Usage: %s <port #> <host name>\n",argv[0]);
		return -1;	
	}
	
	int err;
	char * hostname = argv[2];
	char * portnum = argv[1];
	int socketfd;
	char* command = NULL;
	char* argument = NULL;
	const char delimiter[2] = " ";
	char buffer[512];

	// Attempt a connection
	socketfd = getSocket(hostname, portnum);
	if(socketfd < 0) {
		return -1;
	}	
	printf("mftp>");
	// Read from stdin, one line at a time
	while(fgets(buffer, 512, stdin) != NULL) { 
		// Remove the newline at the end and parse the input	
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
			// ls command
			list();
		}
		else if(!strcmp(command, "rls")) {
			// rls command
			serverList(hostname, socketfd);
		}
		else if(!strcmp(command, "cd")) {
			// cd command
			if(argument == NULL) {
				printf("Command error: expecting a parameter\n");
				printf("mftp>");
				continue;
			}
			changeDirectory(argument);
		}
		else if(!strcmp(command, "rcd")) {
			//rcd command
                        if(argument == NULL) {
                                printf("Command error: expecting a parameter\n");
				printf("mftp>");
                                continue;
                        }
			changeServerDirectory(argument, socketfd);
                }
		else if(!strcmp(command, "get")) {
			// get command
                        if(argument == NULL) {
                                printf("Command error: expecting a parameter\n");
				printf("mftp>");
                                continue;
                        }
			getFile(argument, hostname, socketfd);
                }
		else if(!strcmp(command, "put")) {
			// put command
			if(argument == NULL) {
				printf("Command error: expecting a parameter\n");
				printf("mftp>");
				continue;
			}
			putFile(argument, hostname, socketfd);
		}
		else if(!strcmp(command, "show")) {
			// show command
                        if(argument == NULL) {
                                printf("Command error: expecting a parameter\n");
				printf("mftp>");
                                continue;
                        }
			showFileContents(argument, hostname, socketfd);
                }
		else {
			printf("Command '%s' is unknown - ignored\n",command);
		}
		printf("mftp>");
	}
	printf("EOF detected\n");
	quit(socketfd);	
	return 0;
	
}

// This function simply takes a given string, and prints it to a given socket with error checking.
// Returns 0 on success and -1 on error.
int writeToServer(int fd, char* message) {
	int err;
	// printf("**Writing to server: %s", message);
	err = write(fd, message, strlen(message));
	if(err < 0) {
		perror("write to server");
		return -1;
	}
	return 0;
}

// This function reads from a socket until finding a \n character and stores what it read in the given string.
// Also takes the string size to prevent buffer overflow. Returns a 0 on success and -1 on failure.
int readFromServer(int fd, char* storage, int storageSize) {
	char buffer[32] = "";
	int err;
	int index = 0;
	// Keep reading as long as the last char read wasn't a \n
	while(buffer[0] != '\n' && index < storageSize) {
		err = read(fd, buffer, 1);
		if(err < 0) {
			perror("read from server");
			return -1;
		}
		storage[index] = buffer[0];
		index++;
	}
	storage[index] = '\0';
	// printf("**Read from server: %s", storage);
	return 0;
}

// This function will take an error recieved from the server and quickly parse and print it.
// Returns 0 on success and -1 if it was sent not an error message from the server
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
	fprintf(stderr, "Error recieved from server: %s", message);
	return 0;
}

// This function attempts to connect a socket on the given hostname and portnumber.
// Returns the descriptor for the new socket, and a negative value upon an error.
int getSocket(char* hostname, char* portnum) {
	int socketfd, err;
	struct addrinfo hints, *actualdata;

	// printf("Attempting connecton on port: %s\n", portnum);
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_INET;
	// Get info
        err = getaddrinfo(hostname, portnum, &hints, &actualdata);
        if(err != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
                return -2;
        }
	// Get socket
        socketfd = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
	if(socketfd == -1) {
		freeaddrinfo(actualdata);
                perror("Socket");
                return -1;
        }
	// Connect socket
        err = connect(socketfd, actualdata->ai_addr, actualdata->ai_addrlen);
        if(err < 0) {
		freeaddrinfo(actualdata);
                perror("Connect");
                return -1;
        }
	// Free allocated memory on info
	freeaddrinfo(actualdata);
	return socketfd;
}

// This function attempts to set up a data connectoin between the client and the server.
// Returns the descriptor for the ne connection, or a negative number on an error.
int getDataConnection(char* hostname, int mainConnection) {
	char portnum[8];
	int err;
	// Ask the server for a connection
	writeToServer(mainConnection, "D\n");
	char response[512];
	int index = 0;
	// Read the response and parse the portnumber
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
		errorPrint(response);
		return -1;
	}
	// Get a socket using the info
	int newSocketfd = getSocket(hostname, portnum);
	// printf("new port: %s\n",portnum);
	return newSocketfd;	
}

// This function containst the put command. It communicates with the server accross the main connection
// and attempts to transfer a file over the data connection from the given pathname.
// Returns 0 on success and -1 on failure.
int putFile(char* path, char* hostname, int mainConnection){
	// Open file
	int err;
	char name[256] = "";
	char buffer[512];
	char message[512];
	char response[512];
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
	strcpy(message,"P");
	strcat(message, name);
	strcat(message, "\n");
	writeToServer(mainConnection, message);

	// Check response
	readFromServer(mainConnection, response, 512);
	if(response[0] == 'A') {
		// printf("Server acknowledged put\n");
	}
	else if(response[0] == 'E') {
		errorPrint(response);
		close(fd);
		return -1;
	}
	else {
		fprintf(stderr, "Server Error: Server sent unexpected response");
		close(fd);
		return -1;
	}
	// Send File accross data connection
	int numRead;
	while( (numRead = read(fd, buffer, 511)) > 0) {
		// printf("numread= %d\n",numRead);
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

// This function represents the get command. It communicates with the server accross the main connection
// and attempts to transfer a file from the server. Returns 0 on success and -1 on an error.
int getFile(char* path, char* hostname, int mainConnection){
	int err;
	int index = 0;
	char name[256] = "";
	char buffer[512];
	char message[512];
	char response[512];
	// Get filename from the path
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
	// Open the file, checking if it already exists
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
	// open the data connection	
	int dataConnection = getDataConnection(hostname, mainConnection);
	// Make the requiest to the server
	strcpy(message, "G");
	strcat(message, path);
	strcat(message, "\n");
	writeToServer(mainConnection, message);
	// Get the response
	readFromServer(mainConnection, response, 512);
        
	if(response[0] == 'A') {
                printf("Server accepted command\n");
        }
        else if(response[0] == 'E') {
		remove(path);
		errorPrint(response);
		close(fd);
                return -1;
        }
	else {
		fprintf(stderr, "Servor Error: Server sent unexpected response\n");
		remove(path);
		close(fd);
		return -1;
	}
	
	// Transfer the file	
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

// This function represnets the quit command. It communicates with the server it is exiting
// Returns a 0 on success and -1 on an error.
int quit(int mainConnection) {
	int err;
	char response[512];
	
	// Send the request to the server and get a response
	writeToServer(mainConnection, "Q\n");
	readFromServer(mainConnection, response, 512);

	if(response[0] == 'A') {
		printf("Successfully quit from server\n");
		close(mainConnection);
		return 0;
	}
	else if(response[0] == 'E') {
		errorPrint(response);
		return -1;
	}
	else {
		fprintf(stderr, "Server error: Server sent unexpected response\n");
		return -1;
	}
	close(mainConnection);
	return 0;
}

// This function represents the show command. It communicates with the server across the main connection
// and attempts to get the files contents from the given path and print it to the client.
// Returns 0 on success and -1 on failure
int showFileContents(char* path, char* hostname, int mainConnection) {
	// Establish data connection
	int err;
	char message[512];
	char response[512];
	int dataConnection = getDataConnection(hostname, mainConnection);
	// Send G command to get file
	strcpy(message, "G");
	strcat(message, path);
	strcat(message, "\n");
	writeToServer(mainConnection, message);
	// Get response
	readFromServer(mainConnection, response, 512);

	if(response[0] == 'A') {
		printf("Server accepted command\n");	
	}
	else if(response[0] == 'E') {
		errorPrint(response);
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

// This function represents the rcd command. It communicates across the main connectoin to tell the server
// where to cd into. Returns 0 on success and -1 on an error
int changeServerDirectory(char* newPath, int mainConnection) {
	int err;
	char message[512];
	char response[512];
	
	// Send command to the server
	strcpy(message, "C");
	strcat(message, newPath);
	strcat(message, "\n");
	writeToServer(mainConnection, message);

	// Get response
	readFromServer(mainConnection, response, 512);

	if(response[0] == 'A') {
		// printf("Successfully changed server directory.\n");
		return 0;
	}
	else if(response[0] == 'E') {
		errorPrint(response);
		return -1;
	}
	else {
		fprintf(stderr, "Error: Server returned unexpected response.\n");
		return -1;
	}

	return 0;
}

// This function represents the cd command. It calls chdir with the given path and includes error checking.
// Returns 0 on success and =1 on failures
int changeDirectory(char* newPath) {
	int err;

	err = chdir(newPath);
	if(err == -1) {
		perror("chdir");
		return -1;
	}
	return 0;
}

// This function represents the rls command. It communicates with the server across the main connection
// and gets the ls info from the data connection.
// Returns 0 on success and -1 on error.
int serverList(char* hostname, int mainConnection) {
	int dataConnection = getDataConnection(hostname, mainConnection);
	int err;
	char response[512];
	// Send command and get response
	writeToServer(mainConnection, "L\n");

	readFromServer(mainConnection, response, 512);

	if(response[0] == 'E') {
		errorPrint(response);
		return -1;
	}
	else if(response[0] == 'A') {
		// Fork into two processes
		pid_t child = fork();
		if(child == -1) {
			perror("Fork");
			return -1;
		}

		if(!child) { // Child process
			// Replace the stdin descriptor with the data connection and exec into more
			dup2(dataConnection,0);
                        execlp("more","more","-20", (char *) NULL);
                        perror("execlp");
                        return -1;
		}
		else { // Parent
			// Wait for the child and return
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

// This function repressents the ls command. It forks twice and prints the directory to the client.
// Retrns 0 on success and -1 on failure.
int list() {
	int wstatus;
	int err;

	// Fork for the first time
	pid_t id = fork();
	if(id == -1) {
		perror("Fork");
		return -1;
	}

	if(!id) {// Child
		int fd[2];

		// Setup a pipe
		err = pipe(fd);
		if(err == -1) {
			perror("pipe");
			return -1;
		}

		// Fork again into two
		pid_t youngerChild = fork();
		if(youngerChild == -1) {
			perror("Fork");
			return -1;
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
				return -1;
			}

			err = dup2(fd[0],0);
			if(err == -1) {
				perror("dup2");
				return -1;
			}

			execlp("more", "more", "-20", (char *) NULL);
			perror("execvp child");
			return -1;

		}
		else {// Younger Child
			// Here we execute ls and pipe it to the older child
			err = close(fd[0]);
			if(err == -1) {
                                perror("close");
                                return -1;
                        }

                        err = dup2(fd[1],1);
			if(err == -1) {
                                perror("dup2");
                                return -1;
                        }

			execlp("ls", "ls", "-a", "-l", (char *) NULL);
                        perror("execvp child");
                        return -1;

		}
	}
	// Parent waits for the child then returns.
	err = wait(NULL);
	if(err == -1) {
		perror("wait");
		return -1;
	}
	
	return 0;
}
