/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 3072
#define FRAMESIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int sendPackage(int sockfd, char *buf, int size, struct sockaddr *address, int *len){
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

int receivePackage(int sockfd, char *buf, int size, struct sockaddr *address, int *len){
	int n = 0;
	
	int times = 0;
	while(times<10){
		bzero(buf, size);
		n = recvfrom(sockfd, buf, size, 0, address, len);
		if (n>=0) break;
		times++;
	}
	
	
	if (n < 0) return -1;
	
	int readbyte = n;
	
	n = sendto(sockfd, "ack", 3, 0, address, *len);
	if (n < 0) error("ERROR in sendto");
	
	//printf("readbytes %d\n", readbyte - FRAMESIZE * 2);
    
    return readbyte - FRAMESIZE * 2;
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));
    
    struct timeval outtime;
    outtime.tv_sec = 3;
    outtime.tv_usec = 0;
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &outtime, sizeof(outtime)) < 0){
        error("ERROR timeout socket");
    }

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
      do{
          n = receivePackage(sockfd, buf, BUFSIZE, (struct sockaddr *) &clientaddr, &clientlen);
      }while (n<=0);
    
    char message[128] = {0};
    strncpy(message, buf + FRAMESIZE*2, n);
    
    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    //~ hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  //~ sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    //~ if (hostp == NULL)
      //~ error("ERROR on gethostbyaddr");
    //~ hostaddrp = inet_ntoa(clientaddr.sin_addr);
    //~ if (hostaddrp == NULL)
      //~ error("ERROR on inet_ntoa\n");
    //~ printf("server received datagram from %s (%s)\n", 
	   //~ hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(message), n, message);
    
    char *token = strtok(message, " ");
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
    
    if (strcmp (command, "get") == 0){
        char nFile[128] = {0};
        strcpy(nFile, "./FileStorage");
        strcat(nFile, "/");
        strcat(nFile, filename);
        
		FILE *fp = fopen(nFile, "r");
		if (fp == NULL){
			printf("open fail\n");
		}else{
			fseek(fp, 0, SEEK_END);
			long file_size = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			
			long packageNums = file_size / FRAMESIZE + 1;
			
			long i;
			for(i = 1; i <= packageNums ; i++){
				bzero(buf, FRAMESIZE);
				int readbits = fread(buf, 1, FRAMESIZE,fp);
				
				char *send_bag = (char *)malloc(BUFSIZE);
				bzero(send_bag, BUFSIZE);
				
				
				memcpy(&send_bag[0], &packageNums, sizeof(long));
				memcpy(&send_bag[FRAMESIZE], &i, sizeof(long));
				memcpy(&send_bag[FRAMESIZE * 2], buf, readbits);
				
				int ret = sendPackage(sockfd, send_bag, BUFSIZE - (FRAMESIZE - readbits), (struct sockaddr *) &clientaddr, &clientlen);
				if (ret < 0){
					printf("Server send package with fault\n");
					break;
				}
				
			}
			printf("Done with sending file\n");
			fclose(fp);
			
		}
    }else if (strcmp (command, "put") == 0){
        char nFile[128] = {0};
        strcpy(nFile, "./FileStorage");
        strcat(nFile, "/");
        strcat(nFile, filename);
        
        FILE *fp = fopen(nFile, "w+");
        if (fp == NULL){
            printf("open fail\n");
        }else{
            int writtenBagNum = -1;
            long perscale = 10;
            long oldper = -1;
            while(1){
                n = receivePackage(sockfd, buf, BUFSIZE, (struct sockaddr *) &clientaddr, &clientlen);
                if (n < 0){
                    fclose(fp);
                    remove(filename);
                    error("");
                }
                int total_bag = *(long*)buf;
                int cur_bag = *(long*)&buf[FRAMESIZE];
                
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
        }
    }else if (strcmp (command, "ls") == 0){
        char nFile[128] = {0};
        strcpy(nFile, "ls ");
        strcat(nFile, "./FileStorage");
        strcat(nFile, "/");
        strcat(nFile, filename);
        
        FILE *fstream = NULL;
        char result[1024];
        memset(result, 0, sizeof(result));
        
        fstream = popen(nFile,"r");
        
        
        int readbits = fread(result, sizeof(char), sizeof(result), fstream);
        
        bzero(buf, BUFSIZE);
        memcpy(&buf[FRAMESIZE*2],result ,readbits);
        
        int ret = sendPackage(sockfd, buf, BUFSIZE - (FRAMESIZE - readbits), (struct sockaddr *) &clientaddr, &clientlen);
        if (ret < 0){
            printf("Server send package with fault\n");
            break;
        }
    
        pclose(fstream);
    }else if (strcmp(command, "exit") == 0){
        close(sockfd);
        exit(0);
    }else if (strcmp(command, "delete") == 0){
        char nFile[128] = {0};
        strcpy(nFile, "./FileStorage");
        strcat(nFile, "/");
        strcat(nFile, filename);
        
        remove(nFile);
    }
  }
}
