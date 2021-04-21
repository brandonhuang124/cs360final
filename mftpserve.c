#include <stdio.h>


int main(char argc, char** argv){	
	int err, listenfd;
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
	char buffer[512];

	while(1) {
		connectfd = accept(litenfd, (struct sockaddr*) &clientAddress, &length);
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
		int childpid = fork();
		if(pid == 0) { // Were a server child which handles connection.s
			// Do server stuff
			// Keep waiting for commands
			while(1) {
				// read from the pipe
				// Parse command after recieving new line
				// Do command
			}
		}
		// Maybe wait for childs after a threshhold?
	}
	return 0;
}
