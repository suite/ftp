#ifndef UTIL_H
#define UTIL_H
#include <stddef.h>
int createFilePacket(char *buf, char *filename, int BUFSIZE, int command, int read_offset);
int readFilePacketToFile(char *buf);
void print_hex(char *buf, size_t len);

#endif