CC=gcc
CXX=g++
LD=gcc

TARGET=main
CFLAGS=-Wall -O0 -ggdb -static
LDFLAGS=-fPIC -lpthread -lm -lutil -static
OBJS=main.o sha1.o base64.o pty.o ws.o http.o frontend.o

.PHONY: clean

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

frontend.cpp: frontend.html
	xxd -i frontend.html > frontend.cpp

clean:
	rm -rf $(OBJS) $(TARGET) frontend.cpp
