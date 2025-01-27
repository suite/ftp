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
#include <sys/time.h>

#define BUFSIZE 1024



/*
commands:
get [file_name]
put [file_name]
delete [file_name]
ls
exit

1. verify user input is valid format
2. send command + any necessary data
3. wait for server response
 - if takes too long, send err message to stderr
 - stdout result of operation
4. for exit command, exit gracefully after server ack
*/


/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

/*
 * verify_command - parse and verify commands from user
 */
int verify_command(char *buf) {
  // hacky but just ignore delete since we use plain text and need filename
  if (strncmp(buf, "delete ", 7) == 0) {
    return 1;
  }

  char *token = strtok(buf, " \n");
  if (!token) return -1;

  if (strcmp(token, "ls") == 0) {
    return 1;
  } else if (strcmp(token, "exit") == 0) {
    return 1; 
  } else if (strcmp(token, "put") == 0) {
    // get file name
    char *filename = strtok(NULL, "\n");
    if(!filename) return -1; 

    // read file (2000bytes)
    FILE *fp = fopen(filename, "r");
    if(fp == NULL) return -1;
    
    // // get size
    if(fseek(fp, 0, SEEK_END) < 0) return -1;
    long size = ftell(fp);
    if(size < 0) return -1;
    if(fseek(fp, 0, SEEK_SET));

    printf("Putting File: %s Size: %ld\n", filename, size);

    if(size >= BUFSIZE) {
      printf("FILE TOO BIG!!!\n");
      return -1;
    }

    // Setup packet
    size_t filename_l = strlen(filename);
    int arguments = 8;

    // copy filename first so we dont overwrite  
    strncpy(buf+arguments, filename, 255);

    // Overwrite rest with file contents
    int bytes_used = arguments+filename_l;
    size_t bytes_read = fread(buf+bytes_used, 1, BUFSIZE-bytes_used, fp);

    buf[0] = 0x00; // [0] 0x00 = put command
    
    buf[1] = 0x00; // [1]/[2] 0x00 00 = counter [0,65535]
    buf[2] = 0x00;

    buf[3] = 0x00; // [3]/[4] 0x00 00 = count to [0,65535]
    buf[4] = 0x00;

    buf[5] = (bytes_read >> 8) & 0xFF; // [5]/[6] 0x00 00 = bytes read [0,65535]
    buf[6] = bytes_read & 0xFF;        // big endian

    buf[7] = filename_l; // [3] SET TO BYTE LENGTH OF filename [0,255]

    
    printf("read %ld bytes\n", bytes_read);

    // Final packet size
    bytes_used += bytes_read;
    printf("Final packet size %d\n", bytes_used);

    // put bytes_read into [5] and [6]



    print_hex(buf, BUFSIZE);

   

    // make sure to close
    if(fclose(fp) < 0) return -1; 

    // num sends = 2000/BUFSIZE (1024)
    // num sends 2
    // put 1 2 [first chunk of data]
    // wait for server ack
    // put 2 2 [second and final chunk of data]
    // wait for server ack
    // transferred

    // set pending_message = 1

    return 1; 
  } 

  return -1;
}

/*
 *
 */
void print_hex(char *buf, size_t len) {
  printf("hex: ");
  for (int i = 0; i < len; i++) {
      // prevent sign extension (unsigned char)
      printf("%02X ", (unsigned char)buf[i]);
  }

  printf("\n");
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    int pending_response = 0;
    struct timeval timeout={5,0}; // Timeout for 5 seconds

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

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /*
     * Set a timeout for the socket
     * from: https://stackoverflow.com/questions/16163260/setting-timeout-for-recv-fcn-of-a-udp-socket
     */
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));


    /* build the server's Internet address */
    // Make room for serveraddr
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;

    
    // Copy server->h_addr (first host address) to &serveraddr.sin_addr.s_addr
    bcopy((char *)server->h_addr_list[0], 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);

    // Set port, use htons, host to network byte order
    serveraddr.sin_port = htons(portno);
    // set server len to size of serveraddr
    serverlen = sizeof(serveraddr);

    // Send info
    printf("Commands:\nget [file_name]\nput [file_name]\ndelete [file_name]\nls\nexit\n");


    while (1) {

      // states: pending_response, pending_message, idle

      if (pending_response) {
        // wait for full response
        n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
        if (n < 0) // TODO: send better msg (most likely time out)
          error("ERROR in recvfrom");
       
        printf("\n[Server]: %s\n", buf);

        // User has exited, server is saying goodbye.
        if (strcmp(buf, "goodbye") == 0) {
          printf("Server ack exit, closing...\n");
          close(sockfd);
          exit(0);
        }

        pending_response = 0;
        continue;
      }

      // Send something to 

      /* get a message from the user */
      // Zero out buf
      bzero(buf, BUFSIZE);
      printf("Please enter command:\n");
      // Inject buf with stdin 
      fgets(buf, BUFSIZE, stdin);

      // Verify valid command
      if (verify_command(buf) < 0) {
        fprintf(stderr,"ERROR, no such command %s\n", buf);
        continue;  
      }

      // send command
      //  * TODO: dont need to send BUFSIZE everytime
      n = sendto(sockfd, buf, BUFSIZE, 0, &serveraddr, serverlen);
      if (n < 0) 
        error("ERROR in sendto");

      // wait for resposne
      pending_response = 1;
      
    }
}

/*
client - send command

*/
