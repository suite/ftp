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
// TODO: handle ugly new lines
int verify_command(char *buf) {
  if (strcmp(buf, "ls\n") == 0) {
    return 1;
  } else if (strcmp(buf, "exit\n") == 0) {
    return 1; 
  } else if (strncmp(buf, "delete ", 7) == 0) { // starts with "delete "
    // char *filename = buf + 7;

    return 1; 
  } 

  return -1;
}

/*
 *
 */
void print_hex(char *buf) {
  printf("hex: ", buf);
  int len = strlen(buf);

  for (int i = 0; i < len; i++) {
      printf("%02X ", buf[i]);
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
        if (n < 0) 
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
        fprintf(stderr,"ERROR, no such command %s", buf);
        continue;  
      }

      // send command
      n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
      if (n < 0) 
        error("ERROR in sendto");

      // wait for resposne
      pending_response = 1;
      
    }
}

/*
client - send command

*/
