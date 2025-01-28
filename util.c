#include "util.h"
#include <stdio.h>

void print_hex(char *buf, size_t len) {
  printf("hex: ");
  for (int i = 0; i < len; i++) {
      // prevent sign extension (unsigned char)
      printf("%02X ", (unsigned char)buf[i]);
  }

  printf("\n");
}

int createFilePacket(char *buf, char *filename, int BUFSIZE, int command, int read_offset) {
    if(!filename) return -1; 
    
    // read file in binary mode
    FILE *fp = fopen(filename, "rb");
    if(fp == NULL) return -1;

    // get size
    if(fseek(fp, 0, SEEK_END) < 0) return -1;
    long size = ftell(fp);
    if(size < 0) return -1;
    // seek to read_offset (starts at 0, increases if file cannot fit in 1 buffer)
    if(fseek(fp, read_offset, SEEK_SET) < 0) return -1; 

    // Setup packet
    size_t filename_l = strlen(filename);
    int arguments = 8;

    // copy filename first so we dont overwrite  
    strncpy(buf+arguments, filename, 255);

    int bytes_used = arguments+filename_l;
    int packet_bufsize = BUFSIZE-bytes_used;

    // we'll have to send more then one packet...
    int count_to = 0x00;
    if(size > packet_bufsize) {
        count_to = (int)((size + packet_bufsize - 1) / packet_bufsize);
        printf("File size (%d) bigger than buffer size %d bytes. count_to %d\n", size, packet_bufsize, count_to);
    } // TODO :continue here

    // Overwrite rest with file contents

    size_t bytes_read = fread(buf+bytes_used, 1, BUFSIZE-bytes_used, fp);
    printf("Read %d bytes\n", bytes_read);
    // 
   
    buf[0] = command; // [0] command
    
    buf[1] = 0x00; // [1]/[2] 0x00 00 = counter [0,65535]
    buf[2] = 0x00;

    buf[3] = (count_to >> 8) & 0xFF; // [3]/[4] 0x00 00 = count to [0,65535]
    buf[4] = count_to & 0xFF;

    buf[5] = (bytes_read >> 8) & 0xFF; // [5]/[6] 0x00 00 = bytes read [0,65535]
    buf[6] = bytes_read & 0xFF;        // big endian

    buf[7] = filename_l; // [3] SET TO BYTE LENGTH OF filename [0,255]

    print_hex(buf, BUFSIZE);

    if(fclose(fp) < 0) return -1; 

    // send the new offset
    return read_offset + bytes_read;;
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