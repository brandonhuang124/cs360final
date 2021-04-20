#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int list();
socketfd getDataConnection(char*, int);

int main(char argc, char ** argv){
	if(argc < 3){
		printf("Usage: %s <port #> <host name>\n",argv[0]);
		return -1;	
	}
	char * hostname = argv[2];
	char * portnum = argv[1];
	int socketfd;
	struct addrinfo hints, *actualdata;
	list();
	return 0;
	memset(&hints, 0, sizeof(hints));
	int err;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	err = getaddrinfo(hostname, portnum, &hints, &actualdata);
	if(err != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return -1;
	}
	socketfd = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
	if(socketfd == -1){
		perror("Socket");
		return -errno;
	}
	err = connect(socketfd, actualdata->ai_addr, actualdata->ai_addrlen);
	if(err < 0){
		perror("Connect");
		return -errno;
	}
	char buffer[100] = "Q\n";
	printf("writing %s\n", buffer);
	if (write(socketfd, buffer,100) < 0){
		perror("Write: ");
		return -errno;
	}
	fflush(stdout);
	printf("reading\n");
	if (read(socketfd, buffer, 1) < 0){
		perror("Read: ");
		return -errno;
	}
	if(buffer[0] == 'A'){
		printf("success\n");
	}
	else printf ("error\n");
	return 0;
	
}

int getSocket(char* hostname, char* portnum){
	int socketfd;
	struct addrinfo hints, *actualdata;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_INET;
        err = getaddrinfo(hostname, portnum, &hints, &actualdata);
        if(err != 0){
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
                return -2;
        }
        socketfd = socket(actualdata->ai_family, actualdata->ai_socktype, 0);
	if(socketfd == -1){
                perror("Socket");
                return -errno;
        }
        err = connect(socketfd, actualdata->ai_addr, actualdata->ai_addrlen);
        if(err < 0){
                perror("Connect");
                return -errno;
        }
	return socketfd;
}

int getDataConnection(char* hostname, int mainConnection){
	char buffer[8] = "D\n";
	char portnum[8];
	int err;
	err = write(mainConnection, buffer, 8);
	if (err < 0) {
		perror("Write");
		return -1;
	}
	err = read(mainConnection, buffer, 8);
	if (err < 0) {
		perror("Read");
		return -1;
	}
	if(buffer[0] == 'D'){
		int index = 1;
		while(buffer[index] != '\n' && index < 8){
			portnum[index - 1] = buffer[index];
			index++;
		}
		portnum[index - 1] = '\0';
	}
	else {
		return -1;
	}
	int newSocketfd;
	struct addrinfo hints, *actualdata;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	err = getaddrinfo(hostname, portnum, &hints, &actualdata);
	if(err != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return -2;
	}
	newSocketfd = socket(actualdata->
	
}

int list(){
	int wstatus;
	int err;

	pid_t id = fork();
	if(id == -1){
		perror("Fork");
		return errno;
	}
	if(!id){// Child
		int fd[2];
		err = pipe(fd);
		if(err == -1){
			perror("pipe");
			return errno;
		}
		pid_t youngerChild = fork();
		if(youngerChild == -1){
			perror("Fork");
			return errno;
		}
		if(youngerChild){// Older Child
			// Here we get the ls output from the younger child and exec more
			err = wait(NULL);
			if(err == -1){
                		perror("wait");
        			return -1;
			}
			err = close(fd[1]);
			if(err == -1){
				perror("close");
				return errno;
			}
			err = dup2(fd[0],0);
			if(err == -1){
				perror("dup2");
				return errno;
			}
			char* args[2];
			args[0] = "-d";
			args[1] = "-1";
			execlp("more", "more", "-f", (char *) NULL);
			perror("execvp child");
			return -1;

		}
		else {// Younger Child
			// Here we execute ls and pipe it to the older child
			err = close(fd[0]);
			if(err == -1){
                                perror("close");
                                return errno;
                        }
                        err = dup2(fd[1],1);
			if(err == -1){
                                perror("dup2");
                                return errno;
                        }
			execlp("ls", "ls", "-l", (char *) NULL);
                        perror("execvp child");
                        return -1;

		}
	}
	err = wait(NULL);
	printf("done\n");
	if(err == -1){
		perror("Waitpid");
		return -1;
	}
	if(err == -1){
		perror("wait");
		return -1;
	}
	
	return 0;
}
