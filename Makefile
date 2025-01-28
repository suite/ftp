CC = gcc
CFLAGS = -Wall -g
DEPS = util.h
OBJ = util.o

all: server client

server: udp_server.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

client: udp_client.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o server client 