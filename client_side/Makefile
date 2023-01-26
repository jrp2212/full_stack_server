CC  = gcc
CXX = g++

CFLAGS   = -g -Wall $(INCLUDES)
CXXFLAGS = -g -Wall $(INCLUDES)

http-client: http-client.o
	$(CC) $(CFLAGS) http-client.o -o http-client
http-client.o: http-client.c
	$(CC) -c $(CFLAGS) http-client.c
	

.PHONY: clean
clean:
	rm -f *.o *~ http-client

.PHONY: all
all: clean http-client
