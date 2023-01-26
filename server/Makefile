CC  = gcc
CXX = g++

CFLAGS   = -g -Wall $(INCLUDES)
CXXFLAGS = -g -Wall $(INCLUDES)

http-server: http-server.o
	$(CC) $(CFLAGS) http-server.o -o http-server
http-server.o: http-server.c
	$(CC) -c $(CFLAGS) http-server.c
	

.PHONY: clean
clean:
	rm -f *.o *~ http-server

.PHONY: all
all: clean http-server
