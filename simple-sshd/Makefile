CC = gcc
CCX = g++
CFLAGS = -Wall -O2 -std=c++11
OBJS = server.o
PROG = server
LDFLAGS = -lssh

all: $(PROG)

$(PROG): $(OBJS)
	$(CCX) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

server.o: server.cpp
	$(CCX) $(CFLAGS) -c server.cpp

clean:
	rm -f *.o $(PROG)
