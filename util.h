#ifndef UTIL_H
#define UTIL_H

int createFilePacket(char *buf, char *filename, int BUFSIZE, int command);
int readFilePacketToFile(char *buf);

#endif