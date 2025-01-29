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
#include "util.h"

#define BUFSIZE 32768

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/*
 * execute_buf - parse and execute commands from user
 */
int execute_buf(char *buf, FILE **fp, uint64_t *write_offset) {
  char *token = strtok(buf, " \n");

  // Execute ls
  if (token && strcmp(token, "ls") == 0) {
    // clear out buf to put response in
    bzero(buf, BUFSIZE);

    printf("ls command detected on server\n");
   
    FILE *fp = popen("ls -l", "r");
    if (fp == NULL) {
      fprintf(stderr,"ERROR, could not run ls command\n");
      return -1;
    }

    // read BUFSIZE amount of size 1 bytes into buf
    size_t bytes_read = fread(buf, 1, BUFSIZE, fp);
    printf("read %ld bytes\n", bytes_read);
    printf("new buf:\n%s\n", buf);
    
    if (pclose(fp) == -1) {
       fprintf(stderr,"ERROR, could not close ls command\n");
       return -1;
    }

    return 1;
  } else if (token && strcmp(token, "exit") == 0) {
    bzero(buf, BUFSIZE);
    strncpy(buf, "goodbye", BUFSIZE - 1);
    return 1; 
  } else if (token && strcmp(token, "delete") == 0) {
    char *filename = strtok(NULL, "\n"); 
    if(!filename) return -1;

    char* result;
    if(remove(filename) == 0) {
      result = "Deleted file";
    } else {
      result = "Error deleting file";
    }

    // clear buf and put result inside
    bzero(buf, BUFSIZE);
    strncpy(buf, result, BUFSIZE - 1);
    return 1; 
  } else if (token && strcmp(token, "get") == 0) {
    char *filename = strtok(NULL, "\n"); 
    return createFilePacket(buf, fp, filename, BUFSIZE, 0x01, 0, 0);// TODO: patch impl
  } else if (buf[0] == 0x00) { // client puts file

    int network_order_value;
    memcpy(&network_order_value, &buf[1], 2);
    int counter = ntohs(network_order_value);

    int network_order_value_2;
    memcpy(&network_order_value_2, &buf[3], 2);
    int count_to = ntohs(network_order_value_2);

    // Create file
    int bytes_written = readFilePacketToFile(buf, fp, *write_offset);
    *write_offset += bytes_written;
    printf("write offset %d\n", *write_offset);

   
   // if counter == count_to, reset fp 

    // if (new_fp < 0) {
    //   // Could not read file packet, send back error
    //   printf("ERROR READING FILE PACKET ON SERVER\n");
    // };

    


    // // clear buf and put result inside
    bzero(buf, BUFSIZE);
    buf[0] = 0x21; // ack packet
    buf[1] = (counter >> 8) & 0xFF; // send back the counter client gave us
    buf[2] = counter & 0xFF;

    buf[3]  = (*write_offset >> 56) & 0xFF; // send the total bytes written
    buf[4]  = (*write_offset >> 48) & 0xFF; // we need to store a big number..
    buf[5]  = (*write_offset >> 40) & 0xFF; // fix in future, dont need to send in packet
    buf[6]  = (*write_offset >> 32) & 0xFF;
    buf[7]  = (*write_offset >> 24) & 0xFF;
    buf[8]  = (*write_offset >> 16) & 0xFF;
    buf[9]  = (*write_offset >>  8) & 0xFF;
    buf[10] = (*write_offset      ) & 0xFF;
  
    
    buf[11] = (count_to >> 8) & 0xFF; // send the total bytes written
    buf[12] = count_to & 0xFF;
    // send back ack packet that we read packet
    // strncpy(buf, "Put success", BUFSIZE - 1);

    // close fp?
    if (counter >= count_to) {
      printf("server is done rec.\n");
      if(fclose(*fp) < 0) {
        printf("COULD NOT CLOSE FILE ALERT!!!!!!!!!\n\n\n");
      }
    }
    return 1; 
  }

  return -1;
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

  /// reset after every file
  FILE *fp = NULL; /* active file buffer */
  uint64_t write_offset = 0;
  ///

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

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // set host ip
  serveraddr.sin_port = htons((unsigned short)portno); // set port

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /*
    
  */
  printf("Server started, listening on port %d\n", portno);

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    // pending_response, pending_message, idle

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    // Empty out message buf
    bzero(buf, BUFSIZE);
    // Inject n with client message
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %ld/%d bytes: %s\n", strlen(buf), n, buf);


    printf("Executing command %s\n", buf);
    
    // execute buf runs command and puts response back into buf
    if (execute_buf(buf, &fp, &write_offset) < 0) {
      fprintf(stderr,"ERROR, could not execute command\n");
      // TODO: send back error message
      continue;  
    }
    
    /* 
     * sendto: echo the input back to the client 
     * TODO: dont need to send BUFSIZE everytime
     */
    n = sendto(sockfd, buf, BUFSIZE, 0, 
	       (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }
}
