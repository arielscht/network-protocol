CFLAGS = -Wall -g -lm -lpthread

NETWORK_OBJECTS = network.o socket.o protocol.o utils.o interface.o

all: network

network: $(NETWORK_OBJECTS)
	gcc $(NETWORK_OBJECTS) -o network $(CFLAGS)

network.o: network.c
	gcc -c network.c $(CFLAGS)

server.o: server.c
	gcc -c server.c $(CFLAGS)

socket.o: socket.c socket.h
	gcc -c socket.c $(CFLAGS)

interface.o: interface.c interface.h
	gcc -c interface.c $(CFLAGS)

protocol.o: ./protocol/protocol.c ./protocol/protocol.h
	gcc -c ./protocol/protocol.c $(CFLAGS)

utils.o: ./protocol/utils.c ./protocol/utils.h
	gcc -c ./protocol/utils.c $(CFLAGS)

clean:
	rm -f *.o

purge:	clean
	rm -f client
	rm -f server