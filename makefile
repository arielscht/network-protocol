CFLAGS = -Wall

CLIENT_OBJECTS = client.o socket.o protocol.o
SERVER_OBJECTS = server.o socket.o protocol.o

all: client server

server: $(SERVER_OBJECTS)
	gcc $(SERVER_OBJECTS) -o server

client: $(CLIENT_OBJECTS)
	gcc $(CLIENT_OBJECTS) -o client
	
client.o: client.c
	gcc -c client.c $(CFLAGS)

server.o: server.c
	gcc -c server.c $(CFLAGS)

socket.o: socket.c socket.h
	gcc -c socket.c $(CFLAGS)

protocol.o: protocol.c protocol.h
	gcc -c protocol.c $(CFLAGS)

clean:
	rm -f *.o

purge:	clean
	rm -f client
	rm -f server