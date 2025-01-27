#include "util.h"
#include <stdio.h>

int createFilePacket(char *buf, char *filename, int BUFSIZE, int command) {
    if(!filename) return -1; 
    
    // read file
    FILE *fp = fopen(filename, "r");
    if(fp == NULL) return -1;

    // get size
    if(fseek(fp, 0, SEEK_END) < 0) return -1;
    long size = ftell(fp);
    if(size < 0) return -1;
    if(fseek(fp, 0, SEEK_SET));

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

    buf[0] = command; // [0] command
    
    buf[1] = 0x00; // [1]/[2] 0x00 00 = counter [0,65535]
    buf[2] = 0x00;

    buf[3] = 0x00; // [3]/[4] 0x00 00 = count to [0,65535]
    buf[4] = 0x00;

    buf[5] = (bytes_read >> 8) & 0xFF; // [5]/[6] 0x00 00 = bytes read [0,65535]
    buf[6] = bytes_read & 0xFF;        // big endian

    buf[7] = filename_l; // [3] SET TO BYTE LENGTH OF filename [0,255]

    if(fclose(fp) < 0) return -1; 

    return 1;
}

int readFilePacketToFile(char *buf) {
    int arguments = 8;
    int counter_1 = buf[1];
    int counter_2 = buf[2];

    int count_to_1 = buf[3];
    int count_to_2 = buf[4];

    int bytes_read_1 = buf[5];
    int bytes_read_2 = buf[6];

    // read in buf[5] and buf[6] for bytes read
    int network_order_value;
    memcpy(&network_order_value, &buf[5], 2);

    int bytes_read = ntohs(network_order_value);

    int filename_l = buf[7];

    char *filename = malloc(filename_l + 1); // +1 for null terminator
    strncpy(filename, buf + arguments, filename_l);
    filename[filename_l] = '\0';

    // read file to disk
    FILE *fp = fopen(filename, "wb");
    if(fp == NULL) return -1; 

    int bytes_used = arguments+filename_l;
    size_t bytes_written = fwrite(buf+bytes_used, 1, bytes_read, fp);
    printf("Wrote %ld bytes\n", bytes_written);

    if(fclose(fp) < 0) return -1;

    return 1;
}