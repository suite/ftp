#ifndef UTIL_H
#define UTIL_H
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
int createFilePacket(char *buf, FILE **fp, char *filename, int BUFSIZE, int command, uint64_t read_offset, int counter);
int readFilePacketToFile(char *buf, FILE **fp, uint64_t write_offset);
void print_hex(char *buf, size_t len);
int createAckPacket(char *buf, FILE **file, int BUFSIZE, uint64_t *write_offset);

#endif