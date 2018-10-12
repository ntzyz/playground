CC=gcc
CXX=g++
LD=gcc

SDL_CFLAGS=$(shell pkg-config sdl --cflags)
SDL_LDFLAGS=$(shell pkg-config sdl --libs)
FLAC_CFLAGS=$(shell pkg-config flac --cflags)
FLAC_LDFLAGS=$(shell pkg-config flac --libs)

TARGET=main
CFLAGS=-Wall -O0 -ggdb $(SDL_CFLAGS) $(FLAC_CFLAGS)
LDFLAGS=-fPIC $(SDL_LDFLAGS) $(FLAC_LDFLAGS)
OBJS=main.o

.PHONY: clean

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TARGET)