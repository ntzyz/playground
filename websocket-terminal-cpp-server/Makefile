CC=gcc
CXX=g++
LD=gcc

TARGET=main
CFLAGS=-Wall -O3 -static
LDFLAGS=-fPIC -lpthread -lm -lutil -static
OBJS=main.o sha1.o base64.o pty.o

.PHONY: clean

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TARGET)
