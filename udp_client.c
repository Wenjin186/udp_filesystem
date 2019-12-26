/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define BUFSIZE 3072
#define FRAMESIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


int sendPackage(int sockfd, char *buf, int size, struct sockaddr_in *address, int *len){
	int n = 0;
	int times = 0;
	char new_buf[BUFSIZE] = {0};
	
	do{
		n = sendto(sockfd, buf, size, 0, address, *len);
		if (n < 0) error("ERROR in sendto");
		
		bzero(new_buf, BUFSIZE);
		n = recvfrom(sockfd, new_buf, size, 0, address, len);
		
		times ++;
		if (times == 10 && n<0) return -1;
		
		
	}while(n<0);
	
	if(strcmp(new_buf, "ack") == 0) return 0;
	
	return -1;
}

int receivePackage(int sockfd, char *buf, int size, struct sockaddr_in *address, int *len){
	int n = 0;
	
    int times = 0;
    while(times<10){
        bzero(buf, size);
        n = recvfrom(sockfd, buf, size, 0, address, len);
        if (n>=0) break;
        times ++;
    }
	
	
	if (n < 0) return -1;
	
	int readbyte = n;
	
	n = sendto(sockfd, "ack", 3, 0, address, *len);
	if (n < 0) error("ERROR in sendto");
    
    return readbyte - FRAMESIZE * 2;
}


int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    
    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
        
    struct timeval outtime;
	outtime.tv_sec = 3;
	outtime.tv_usec = 0;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &outtime, sizeof(outtime)) < 0){
		error("ERROR timeout socket");
	}

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */
    bzero(buf, BUFSIZE);
    printf("Please enter msg: ");
    fgets(buf, BUFSIZE, stdin);
     
    char *token = strtok(buf, " ");
    char command[128] = {0};
    char filename[128] = {0};
    strncpy(command, token, strlen(token));
    
    token = strtok(NULL, " ");
    
    if (token != NULL){	
		strncpy(filename, token, strlen(token));
		int lfile = strlen(filename);
		if (filename[lfile-1] == '\n'){
			filename[lfile-1] = '\0';
		}
	}	

    int lcom = strlen(command);
    if (command[lcom-1] == '\n'){
		command[lcom-1] = '\0';
	}
	
	char message[128] = {0};
	strcpy(message, command);
	strcat(message, " ");
	strcat(message, filename);
	
	char package[BUFSIZE] = {0};
	strncpy(&package[FRAMESIZE*2], message, strlen(message));
    
    /* print the server's reply */
    if (strcmp(command, "get") == 0){
		
		if (strlen(filename) == 0){
			printf("The format of get command is wrong\n");
			exit(0);
		}
		
		/* send the message to the server */
		serverlen = sizeof(serveraddr);
		n = sendPackage(sockfd, package, FRAMESIZE*2+strlen(message), &serveraddr, &serverlen);
		if (n < 0) error("ERROR in sendPackage");
		
		FILE *fp = fopen(filename, "w+");
		
		int writtenBagNum = -1;
		long perscale = 10;
		long oldper = -1;
		while(1){
			n = receivePackage(sockfd, buf, BUFSIZE, &serveraddr, &serverlen);
			if (n < 0){
				fclose(fp);
				remove(filename);
				error("");
			}
			int total_bag = *(long*)buf;
			int cur_bag = *(long*)&buf[FRAMESIZE];
//            printf("total bag %ld\n", total_bag);
//            printf("cur_bag %ld\n", cur_bag);
//            printf("-----------\n");
			
            if (cur_bag != writtenBagNum) {
                fwrite(&buf[FRAMESIZE * 2], 1, n, fp);
                writtenBagNum = cur_bag;
            }
    
            long per = (long)((double)cur_bag/(double)total_bag * 100);
            if (per % perscale == 0 ) {
                 if (oldper != per) {
                    oldper = per;
                    printf("Transmission Percentage : %ld%%\n", per);
                }
             }
			if (cur_bag == total_bag) break;
		}
		
		fclose(fp);
		printf("Success!\n");
		
    }else if(strcmp(command, "put") == 0){
        if (strlen(filename) == 0){
            printf("The format of put command is wrong\n");
            exit(0);
        }
        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        n = sendPackage(sockfd, package, FRAMESIZE*2+strlen(message), &serveraddr, &serverlen);
        if (n < 0) error("ERROR in sendPackage");
        
        FILE *fp = fopen(filename, "r");
        if (fp == NULL){
            printf("open fail\n");
        }else{
            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            
            long packageNums = file_size / FRAMESIZE + 1;
            
            long i;
            long perscale = 10;
            long oldper = -1;
            for(i = 1; i <= packageNums ; i++){
                bzero(buf, FRAMESIZE);
                int readbits = fread(buf, 1, FRAMESIZE,fp);
                
                char *send_bag = (char *)malloc(BUFSIZE);
                bzero(send_bag, BUFSIZE);
            
                memcpy(&send_bag[0], &packageNums, sizeof(long));
                memcpy(&send_bag[FRAMESIZE], &i, sizeof(long));
                memcpy(&send_bag[FRAMESIZE * 2], buf, readbits);
                
                int ret = sendPackage(sockfd, send_bag, BUFSIZE - (FRAMESIZE - readbits), &serveraddr, &serverlen);
                if (ret < 0){
                    printf("Client send package with fault\n");
                    break;
                }
                long per = (long)((double)i/(double)packageNums * 100);
                if (per % perscale == 0 ) {
                    if (oldper != per) {
                        oldper = per;
                        printf("Upload Progress : %ld%%\n", per);
                    }
                }
            }
            fclose(fp);
            printf("Done with uploding file\n");
        }
    }else if(strcmp(command, "ls") == 0){
        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        n = sendPackage(sockfd, package, FRAMESIZE*2+strlen(message), &serveraddr, &serverlen);
        if (n < 0) error("ERROR in sendPackage");
        
        bzero(buf, BUFSIZE);
        n = receivePackage(sockfd, buf, BUFSIZE, &serveraddr, &serverlen);
        if (n < 0){
            error("ls fail");
        }
        printf("%s\n", &buf[FRAMESIZE*2]);
    }else if (strcmp(command, "exit") == 0){
        serverlen = sizeof(serveraddr);
        n = sendPackage(sockfd, package, FRAMESIZE*2+strlen(message), &serveraddr, &serverlen);
        if (n < 0) error("ERROR in sendPackage");
    }else if (strcmp(command, "delete") == 0){
        if (strlen(filename) == 0){
            printf("The format of put command is wrong\n");
            exit(0);
        }
        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        n = sendPackage(sockfd, package, FRAMESIZE*2+strlen(message), &serveraddr, &serverlen);
        if (n < 0) error("ERROR in sendPackage");
        
    }
    return 0;
}
