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

int createFilePacket(char *buf, FILE **fp, char *filename, int BUFSIZE, int command, int read_offset, int counter) {
    if(!filename) { printf("no file name in create packet\n"); return -1; }; 
    
    // read file in binary mode
    if (*fp == NULL) {
        *fp = fopen(filename, "rb");
        if(*fp == NULL) { printf("no fp in create packet\n"); return -1; }; 
    }

    // get size
    if(fseek(*fp, 0, SEEK_END) < 0) return -1;
    long size = ftell(*fp);
    if(size < 0) return -1;
    // seek to read_offset (starts at 0, increases if file cannot fit in 1 buffer)
    if(fseek(*fp, read_offset, SEEK_SET) < 0) return -1; 

    // Setup packet
    size_t filename_l = strlen(filename);
    int arguments = 8;

    // copy filename first so we dont overwrite  
    // strncpy(buf+arguments, filename, 255);
    memcpy(buf + arguments, filename, filename_l);

    // 00 00 08 00 09 00 EF 01
    // 00 00 00 00 09 03 EF 09 

    int bytes_used = arguments+filename_l;
    int packet_bufsize = BUFSIZE-bytes_used;

    // Clear the buffer after the header and filename to prevent garbage data
    // memset(buf + bytes_used, 0, BUFSIZE - bytes_used);
    bzero(buf + bytes_used, BUFSIZE - bytes_used);

    // we'll have to send more then one packet...
    int count_to = 0x00;
    if(size > packet_bufsize) {
        count_to = ((int)((size + packet_bufsize - 1) / packet_bufsize));
        printf("File size (%d) bigger than buffer size %d bytes. count_to %d\n", size, packet_bufsize, count_to);
    } // TODO :continue here

    printf("count_to: %d , counter: %d\n", count_to, counter);

    // Overwrite rest with file contents
    int remaining = size-read_offset;
    size_t bytes_read = fread(buf+bytes_used, 1, (remaining < packet_bufsize) ? remaining : packet_bufsize, *fp);
    printf("Read %d bytes FROM POSITION:%d\n", bytes_read,read_offset);
    // 
   
    buf[0] = command; // [0] command
    
    buf[1] = (counter >> 8) & 0xFF; // [1]/[2] 0x00 00 = counter [0,65535]
    buf[2] = counter & 0xFF;

    buf[3] = (count_to >> 8) & 0xFF; // [3]/[4] 0x00 00 = count to [0,65535]
    buf[4] = count_to & 0xFF;

    buf[5] = (bytes_read >> 8) & 0xFF; // [5]/[6] 0x00 00 = bytes read [0,65535]
    buf[6] = bytes_read & 0xFF;        // big endian

    buf[7] = filename_l; // [3] SET TO BYTE LENGTH OF filename [0,255]

    print_hex(buf, BUFSIZE);

    // TODO: DONT HERE??
    // if(fclose(*fp) < 0) return -1; 

    // send the new offset
    return read_offset + bytes_read;;
}

int readFilePacketToFile(char *buf, FILE **fp, int write_offset) {
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
    // strncpy(filename, buf + arguments, filename_l);
    memcpy(filename, buf + arguments, filename_l);
    filename[filename_l] = '\0';

    // read file to disk
    // if no file yet
    if (*fp == NULL) {
        *fp = fopen(filename, "wb");
        if(*fp == NULL) return -1; 
    }
   
    // Seek to write_offset
    if(fseek(*fp, write_offset, SEEK_SET) < 0) return -1; 

    int bytes_used = arguments+filename_l;
    
    size_t bytes_written = fwrite(buf+bytes_used, 1, bytes_read, *fp);
    printf("Wrote %ld bytes\n", bytes_written);

    // TODO: dont close here..
    // if(fclose(*fp) < 0) return -1;
    fflush(*fp);

    return bytes_written;
}