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

    // old printf("USING BUFSIZE: %d\n", BUFSIZE);

    // old printf("create file packet readoffset:: %" PRIu64 "\n", read_offset);

   // old  printf("filename: %s", filename);

    size_t filename_l_test = strlen(filename);
    // old printf("filename length: %zu\n", filename_l_test);

    // policy3030annotedkek.pdf

    // get size
    if(fseek(*fp, 0, SEEK_END) < 0) { printf("couldnt seek to end\n"); return -1; }; 
   
    // old printf("hello?\n");
   
    long size = ftell(*fp);
    
    // old printf("File size: %ld\n", size);

    if(size < 0)  { printf("size read error\n"); return -1; }; 
    // seek to read_offset (starts at 0, increases if file cannot fit in 1 buffer)
    if(fseek(*fp, read_offset, SEEK_SET) < 0) { printf("could not seek to read_offset\n"); return -1; }; 

    // old printf("File size: %ld\n", size);

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

    if (bytes_used > BUFSIZE) {
        printf("Buffer overflow: header + filename exceeds BUFSIZE.\n");
        return -1;
    }

    
    // Clear the buffer after the header and filename to prevent garbage data
    // memset(buf + bytes_used, 0, BUFSIZE - bytes_used);
    bzero(buf + bytes_used, BUFSIZE - bytes_used);

    // we'll have to send more then one packet...
    int count_to = 0x00;
    if(size > packet_bufsize) {
        count_to = ((int)((size + packet_bufsize - 1) / packet_bufsize));
       // old  printf("File size (%ld) bigger than buffer size %d bytes. count_to %d\n", size, packet_bufsize, count_to);
    } // TODO :continue here

   // old  printf("count_to: %d , counter: %d\n", count_to, counter);

    // Overwrite rest with file contents
    uint64_t remaining = size-read_offset;
    size_t bytes_read = fread(buf+bytes_used, 1, (remaining < packet_bufsize) ? remaining : packet_bufsize, *fp);
   // old  printf("Read %ld bytes FROM POSITION:%ld\n", bytes_read,read_offset);
    // 
   
    buf[0] = command; // [0] command
    
    buf[1] = (counter >> 8) & 0xFF; // [1]/[2] 0x00 00 = counter [0,65535]
    buf[2] = counter & 0xFF;

    buf[3] = (count_to >> 8) & 0xFF; // [3]/[4] 0x00 00 = count to [0,65535]
    buf[4] = count_to & 0xFF;

    buf[5] = (bytes_read >> 8) & 0xFF; // [5]/[6] 0x00 00 = bytes read [0,65535]
    buf[6] = bytes_read & 0xFF;        // big endian

    printf("BYTES READ CLIENT %d\n", bytes_read);

    buf[7] = filename_l; // [3] SET TO BYTE LENGTH OF filename [0,255]

    // print_hex(buf, BUFSIZE);

    // TODO: DONT HERE??
    // if(fclose(*fp) < 0) return -1; 

    // send the new offset
    return read_offset + bytes_read;;
}

int readFilePacketToFile(char *buf, FILE **fp, uint64_t write_offset) {
    int arguments = 8;
    int counter_1 = buf[1];
    int counter_2 = buf[2];

    int count_to_1 = buf[3];
    int count_to_2 = buf[4];

    int bytes_read_1 = buf[5];
    int bytes_read_2 = buf[6];

    // read in buf[5] and buf[6] for bytes read
    // int network_order_value;
    // memcpy(&network_order_value, &buf[5], 2);

    // int bytes_read = ntohs(network_order_value);

    int bytes_read = ((uint16_t)(uint8_t)buf[5]  << 8)  |
              ((uint16_t)(uint8_t)buf[6]);

    printf("BYTES READ %d\n", bytes_read);

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


int createAckPacket(char *buf, FILE **fp, int BUFSIZE, uint64_t *write_offset) {
    int network_order_value;
    memcpy(&network_order_value, &buf[1], 2);
    int counter = ntohs(network_order_value);

    int network_order_value_2;
    memcpy(&network_order_value_2, &buf[3], 2);
    int count_to = ntohs(network_order_value_2);

    // Create file
    uint64_t bytes_written = readFilePacketToFile(buf, fp, *write_offset);
    *write_offset += bytes_written;
    printf("write offset %d\n", *write_offset);

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

    if (counter >= count_to) {
      printf("final ack packet created.\n");

      if(fclose(*fp) < 0) {
        printf("COULD NOT CLOSE FILE ALERT!!!!!!!!!\n\n\n");
      }

      // reset file read vars
      *fp = NULL; /* active file buffer */
      *write_offset = 0;
      return 1;
    }

    return 0;
}