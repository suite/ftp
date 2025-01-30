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
#include "util.h"
#include <inttypes.h> 

#define BUFSIZE 32768



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
int verify_command(char *buf, int *pending_messages, FILE **fp, char **filename) {
  // hacky but just ignore delete/get since we use plain text and need filename
  if (strncmp(buf, "delete ", 7) == 0) {
    return 1;
  } else if (strncmp(buf, "get ", 4) == 0) {
    return 1;
  }

  char *token = strtok(buf, " \n");
  if (!token) return -1;

  if (strcmp(token, "ls") == 0) {
    return 1;
  } else if (strcmp(token, "exit") == 0) {
    return 1; 
  } else if (strcmp(token, "put") == 0) {
    char *filename_token = strtok(NULL, "\n");

    *filename = malloc(strlen(filename_token) + 1);
    if (*filename == NULL) { return -1; }

    strcpy(*filename, filename_token);

    // printf("put read filename %s\n", *filename);
    // 
    int bytes_read = createFilePacket(buf, fp, *filename, BUFSIZE, 0x00, 0, 0);
    if (bytes_read > 0) {
       // set pending_message=1
       *pending_messages = 1;
       return 1;
    }
  } 

  return -1;
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    int pending_response = 0;
    int pending_message = 0;
    struct timeval timeout={5,0}; // Timeout for 5 seconds

    // reset after every file
    FILE *fp = NULL;
    char *filename;
    int counter = 0;

    // 
    uint64_t write_offset = 0;

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
      if (pending_message) {

        printf("In pending message..\n");
        // get next chunk of file
        // send 
        // check if last
        // if so pending_message = 0
        
        // wait for confirmation last packet was sent
        n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
        if (n < 0)
          error("ERROR in pending_message recvfrom, timed out");

        printf("\n[Server]: %s\n", buf);
        // Put success 
        // TODO: get ack packet, send next packet

      // 
      // ack says successfully read up to n bytes
      // createFilePacket(buf, &fp, filename, BUFSIZE, 0x00, 10);

      if (buf[0] == 0x21) {
        printf("Client recieved ack packet\n");
        //
        // int network_order_value;
        // memcpy(&network_order_value, &buf[1], 2);
        // int server_counter = ntohs(network_order_value);

        int server_counter = ((uint16_t)(uint8_t)buf[1]  << 8)  |
              ((uint16_t)(uint8_t)buf[2]);

        if (server_counter == counter) {
          printf("Server read packet. Sending next... Counter: %d\n", counter);

          counter += 1;

          // int network_order_value_2;
          // memcpy(&network_order_value_2, &buf[11], 2);
          // int count_to = ntohs(network_order_value_2);

          int count_to = ((uint16_t)(uint8_t)buf[11]  << 8)  |
              ((uint16_t)(uint8_t)buf[12]);

          if (counter >= count_to) {
            printf("WE READ THE WHOLE FILE!!!!!!!!!\n\n\n");

            // CLOSE FP
      
            if(fclose(fp) < 0) {
              printf("COULD NOT CLOSE FILE ALERT!!!!!!!!!\n\n\n");
            }


            // reset file send vars
            fp = NULL;
            filename = NULL;
            counter = 0;

            pending_message = 0;
            pending_response = 0;
            continue;
          }
          
          uint64_t net_offset = 
              ((uint64_t)(uint8_t)buf[3]  << 56) |  // Most significant byte
              ((uint64_t)(uint8_t)buf[4]  << 48) |
              ((uint64_t)(uint8_t)buf[5]  << 40) |
              ((uint64_t)(uint8_t)buf[6]  << 32) |
              ((uint64_t)(uint8_t)buf[7]  << 24) |
              ((uint64_t)(uint8_t)buf[8]  << 16) |
              ((uint64_t)(uint8_t)buf[9]  << 8)  |
              ((uint64_t)(uint8_t)buf[10]);        // Least significant byte

          uint64_t ack_bytes_written = net_offset;  // 

          // fill buff with new packet
          int s = createFilePacket(buf, &fp, filename, BUFSIZE, 0x00, ack_bytes_written, counter);
          if (s < 0) { error("Couldnt create file packet"); }

          n = sendto(sockfd, buf, BUFSIZE, 0, &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto while sending more file packets");


          // sent packet, we're done
          continue;

        } else {
          error("Incorrect packet order.\n");
        };

      } else {
        error("ERROR Client recieved unknown packet while waiting for ack.\n");
      }

        pending_message = 0;
        pending_response = 0;
        continue;
      }

      if (pending_response) {
        // wait for full response
        n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
        if (n < 0)
          error("ERROR in pending_response recvfrom, timed out");
       
        printf("\n[Server]: %s\n", buf);

        // User has exited, server is saying goodbye.
        if (strcmp(buf, "goodbye") == 0) {
          printf("Server ack exit, closing...\n");
          close(sockfd);
          exit(0);
        }

        // if get response, parse and save file
        if (buf[0] == 0x01) {
          printf("CLIENT GOT ACK PACKET FROM SERVER\n");
          int is_last = createAckPacket(buf, &fp, BUFSIZE, &write_offset);
          

          // send packet
          n = sendto(sockfd, buf, BUFSIZE, 0, &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto while sending more file packets");
         
          pending_response = is_last;
          continue;
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
      if (verify_command(buf, &pending_message, &fp, &filename) < 0) {
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