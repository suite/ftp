#include "util.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h> 
#include <string.h>
#include <stdlib.h>

void print_hex(char *buf, size_t len) {
  printf("hex: ");
  for (int i = 0; i < len; i++) {
      // prevent sign extension (unsigned char)
      printf("%02X ", (unsigned char)buf[i]);
  }

  printf("\n");
}

int createFilePacket(char *buf, FILE **fp, char *filename, int BUFSIZE, int command, uint64_t read_offset, int counter) {
    if(!filename) { printf("no file name in create packet\n"); return -1; }; 
    
    // read file in binary mode
    if (*fp == NULL) {
        *fp = fopen(filename, "rb");
        if(*fp == NULL) { printf("no fp in create packet\n"); return -1; }; 
    }

    // get size
    if(fseek(*fp, 0, SEEK_END) < 0) { printf("couldnt seek to end\n"); return -1; }; 
   
    long size = ftell(*fp);

    if(size < 0)  { printf("size read error\n"); return -1; }; 
    // seek to read_offset (starts at 0, increases if file cannot fit in 1 buffer)
    if(fseek(*fp, read_offset, SEEK_SET) < 0) { printf("could not seek to read_offset\n"); return -1; }; 

    // Setup packet
    size_t filename_l = strlen(filename);
    int arguments = 8;

    // copy filename first so we dont overwrite  
    memcpy(buf + arguments, filename, filename_l);

    int bytes_used = arguments+filename_l;
    int packet_bufsize = BUFSIZE-bytes_used;

    if (bytes_used > BUFSIZE) {
        printf("Buffer overflow: header + filename exceeds BUFSIZE.\n");
        return -1;
    }

    // Clear the buffer after the header and filename to prevent garbage data
    bzero(buf + bytes_used, BUFSIZE - bytes_used);

    // we'll have to send more then one packet...
    int count_to = 0x00;
    if(size > packet_bufsize) {
        count_to = ((int)((size + packet_bufsize - 1) / packet_bufsize));
    } 

    // Overwrite rest with file contents
    uint64_t remaining = size-read_offset;
    size_t bytes_read = fread(buf+bytes_used, 1, (remaining < packet_bufsize) ? remaining : packet_bufsize, *fp);

    buf[0] = command; // [0] command
    
    buf[1] = (counter >> 8) & 0xFF; // [1]/[2] 0x00 00 = counter [0,65535]
    buf[2] = counter & 0xFF;

    buf[3] = (count_to >> 8) & 0xFF; // [3]/[4] 0x00 00 = count to [0,65535]
    buf[4] = count_to & 0xFF;

    buf[5] = (bytes_read >> 8) & 0xFF; // [5]/[6] 0x00 00 = bytes read [0,65535]
    buf[6] = bytes_read & 0xFF;        // big endian

    buf[7] = filename_l; // [3] SET TO BYTE LENGTH OF filename [0,255]

    // send the new offset
    return read_offset + bytes_read;;
}

int readFilePacketToFile(char *buf, FILE **fp, uint64_t write_offset) {
    int arguments = 8;
    int bytes_read = ((uint16_t)(uint8_t)buf[5]  << 8)  |
              ((uint16_t)(uint8_t)buf[6]);

    int filename_l = (uint8_t)buf[7];

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

    fflush(*fp);

    return bytes_written;
}


int createAckPacket(char *buf, FILE **fp, int BUFSIZE, uint64_t *write_offset) {
    int counter = ((uint16_t)(uint8_t)buf[1]  << 8)  |
              ((uint16_t)(uint8_t)buf[2]);

    int count_to = ((uint16_t)(uint8_t)buf[3]  << 8)  |
              ((uint16_t)(uint8_t)buf[4]);

    // Create file
    uint64_t bytes_written = readFilePacketToFile(buf, fp, *write_offset);
    *write_offset += bytes_written;

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

    if ((counter+1) >= count_to) {
      if(fclose(*fp) < 0) {
        printf("Could not close file...\n");
      }

      // reset file read vars
      *fp = NULL; /* active file buffer */
      *write_offset = 0;
      return 0;
    }

    return 1;
}