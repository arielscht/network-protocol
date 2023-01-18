CFLAGS = -Wall -g -lm

CLIENT_OBJECTS = client.o socket.o protocol.o utils.o
SERVER_OBJECTS = server.o socket.o protocol.o utils.o

all: client server

server: $(SERVER_OBJECTS)
	gcc $(SERVER_OBJECTS) -o server $(CFLAGS)

client: $(CLIENT_OBJECTS)
	gcc $(CLIENT_OBJECTS) -o client $(CFLAGS)

client.o: client.c
	gcc -c client.c $(CFLAGS)

server.o: server.c
	gcc -c server.c $(CFLAGS)

socket.o: socket.c socket.h
	gcc -c socket.c $(CFLAGS)

protocol.o: ./protocol/protocol.c ./protocol/protocol.h
	gcc -c ./protocol/protocol.c $(CFLAGS)

utils.o: ./protocol/utils.c ./protocol/utils.h
	gcc -c ./protocol/utils.c $(CFLAGS)

clean:
	rm -f *.o

purge:	clean
	rm -f client
	rm -f server